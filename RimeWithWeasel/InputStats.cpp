#include "stdafx.h"
#include "InputStats.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

#include <logging.h>
#include <WeaselUtility.h>

namespace weasel {

namespace {

constexpr int kKeepYears = 3;  // 保留最近 3 年

// 从 "YYYY-MM-DD" 求当天 0 点的时间戳偏移（用 std::tm）
bool ParseDate(const std::string& ymd, std::tm* tm_out) {
  if (ymd.size() < 10)
    return false;
  std::tm tm = {};
  tm.tm_year = std::atoi(ymd.substr(0, 4).c_str()) - 1900;
  tm.tm_mon = std::atoi(ymd.substr(5, 2).c_str()) - 1;
  tm.tm_mday = std::atoi(ymd.substr(8, 2).c_str());
  tm.tm_hour = 12;  // 用正午避开 DST 边界
  *tm_out = tm;
  return true;
}

std::string FormatDate(const std::tm& tm) {
  char buf[32] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
  return std::string(buf);
}

// 今天往前推 delta 天的日期字符串
std::string DateOffset(int delta_days) {
  std::time_t now = std::time(nullptr);
  std::tm tm = {};
  localtime_s(&tm, &now);
  tm.tm_hour = 12;
  tm.tm_mday -= delta_days;
  std::mktime(&tm);  // 规范化
  return FormatDate(tm);
}

// 极简 JSON 转义（本文件只存 ASCII 键和数字，几乎用不到，留作健壮性）
std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// 从一段文本中读取指定 key 的整数值（极简解析，只认本文件结构）
bool ExtractNumber(const std::string& text,
                   size_t start,
                   const std::string& key,
                   unsigned long long* out) {
  std::string pat = "\"" + key + "\"";
  size_t p = text.find(pat, start);
  if (p == std::string::npos)
    return false;
  p = text.find(':', p + pat.size());
  if (p == std::string::npos)
    return false;
  ++p;
  while (p < text.size() && (text[p] == ' ' || text[p] == '\t'))
    ++p;
  size_t q = p;
  while (q < text.size() && text[q] >= '0' && text[q] <= '9')
    ++q;
  if (q == p)
    return false;
  *out = std::strtoull(text.substr(p, q - p).c_str(), nullptr, 10);
  return true;
}

}  // namespace

// ---------------- 单例 ----------------

InputStats& InputStats::Instance() {
  static InputStats instance;
  return instance;
}

std::wstring InputStats::StatsFilePathW() {
  try {
    fs::path p = WeaselUserDataPath() / L"stats.json";
    return p.wstring();
  } catch (...) {
    return std::wstring();
  }
}

std::string InputStats::StatsFilePath() {
  return wstring_to_string(StatsFilePathW(), CP_UTF8);
}

std::string InputStats::TodayKey() {
  std::time_t now = std::time(nullptr);
  std::tm tm = {};
  localtime_s(&tm, &now);
  return FormatDate(tm);
}

// ---------------- 采集 ----------------

void InputStats::AddKeystroke() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    daily_[TodayKey()].keystrokes += 1;
    dirty_ = true;
  }
  MaybeAutoSave();
}

void InputStats::AddChars(unsigned long long n) {
  if (n == 0)
    return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    daily_[TodayKey()].chars += n;
    dirty_ = true;
  }
  MaybeAutoSave();
}

void InputStats::MaybeAutoSave() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dirty_)
      return;
    std::time_t now = std::time(nullptr);
    if (last_save_time_ != 0 && now - last_save_time_ < kAutoSaveIntervalSec)
      return;
  }
  Save();
}

// ---------------- 持久化 ----------------

bool InputStats::Load() {
  std::wstring path = StatsFilePathW();
  if (path.empty())
    return false;
  std::ifstream ifs(path.c_str(), std::ios::binary);
  if (!ifs.is_open())
    return true;  // 文件不存在：视为空，正常

  std::ostringstream ss;
  ss << ifs.rdbuf();
  std::string text = ss.str();

  std::lock_guard<std::mutex> lock(mutex_);
  daily_.clear();

  // 逐行解析写入时产生的行式 JSON：
  //   "2026-09-24": { "keystrokes": 1024, "chars": 380 }
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    // 快速定位日期键
    size_t q1 = line.find('"');
    if (q1 == std::string::npos)
      continue;
    size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos)
      continue;
    std::string key = line.substr(q1 + 1, q2 - q1 - 1);
    // 日期键格式校验：YYYY-MM-DD
    if (!(key.size() == 10 && key[4] == '-' && key[7] == '-'))
      continue;
    StatsCounters c;
    unsigned long long v = 0;
    if (ExtractNumber(line, q2, "keystrokes", &v))
      c.keystrokes = v;
    if (ExtractNumber(line, q2, "chars", &v))
      c.chars = v;
    daily_[key] = c;
  }
  return true;
}

bool InputStats::Save() {
  std::wstring path = StatsFilePathW();
  if (path.empty())
    return false;

  std::string content;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    // 裁剪：只保留最近 kKeepYears 年
    if (!daily_.empty()) {
      std::string cutoff = DateOffset(kKeepYears * 365 + 1);
      for (auto it = daily_.begin(); it != daily_.end();) {
        if (it->first < cutoff)
          it = daily_.erase(it);
        else
          ++it;
      }
    }

    std::ostringstream oss;
    oss << "{\n  \"version\": 1,\n  \"daily\": {\n";
    bool first = true;
    for (const auto& kv : daily_) {
      if (!first)
        oss << ",\n";
      first = false;
      oss << "    \"" << JsonEscape(kv.first) << "\": { \"keystrokes\": "
          << kv.second.keystrokes << ", \"chars\": " << kv.second.chars << " }";
    }
    oss << "\n  }\n}\n";
    content = oss.str();
  }

  // 原子写：先写 .tmp，再改名
  std::wstring tmp = path + L".tmp";
  {
    std::ofstream ofs(tmp.c_str(), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
      LOG(ERROR) << "InputStats: cannot write tmp file";
      return false;
    }
    ofs.write(content.data(), content.size());
    ofs.flush();
  }
  if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    LOG(ERROR) << "InputStats: rename tmp failed";
    return false;
  }

  // 写盘成功：记录时间并清 dirty（此间若又有新计数会重新置 dirty）
  {
    std::lock_guard<std::mutex> lock(mutex_);
    last_save_time_ = std::time(nullptr);
    dirty_ = false;
  }
  return true;
}

// ---------------- 聚合 ----------------

StatsCounters InputStats::Today() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = daily_.find(TodayKey());
  return it == daily_.end() ? StatsCounters() : it->second;
}

StatsCounters InputStats::Total() const {
  std::lock_guard<std::mutex> lock(mutex_);
  StatsCounters total;
  for (const auto& kv : daily_)
    total += kv.second;
  return total;
}

std::vector<std::pair<std::string, StatsCounters>> InputStats::RecentDays(
    int n) const {
  std::vector<std::pair<std::string, StatsCounters>> result;
  if (n <= 0)
    return result;
  std::lock_guard<std::mutex> lock(mutex_);
  for (int i = 0; i < n; ++i) {
    std::string key = DateOffset(i);
    auto it = daily_.find(key);
    result.emplace_back(key,
                        it == daily_.end() ? StatsCounters() : it->second);
  }
  return result;
}

std::vector<std::pair<std::string, StatsCounters>> InputStats::ByMonth() const {
  std::map<std::string, StatsCounters> agg;  // 有序，便于倒序输出
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : daily_) {
      if (kv.first.size() >= 7)
        agg[kv.first.substr(0, 7)] += kv.second;
    }
  }
  std::vector<std::pair<std::string, StatsCounters>> result(agg.begin(),
                                                            agg.end());
  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<std::pair<std::string, StatsCounters>> InputStats::ByYear() const {
  std::map<std::string, StatsCounters> agg;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : daily_) {
      if (kv.first.size() >= 4)
        agg[kv.first.substr(0, 4)] += kv.second;
    }
  }
  std::vector<std::pair<std::string, StatsCounters>> result(agg.begin(),
                                                            agg.end());
  std::reverse(result.begin(), result.end());
  return result;
}

}  // namespace weasel
