#include "stdafx.h"
#include "InputStats.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>

#include <logging.h>
#include <WeaselUtility.h>

namespace weasel {

namespace {

constexpr int kKeepYears = 3;  // Keep the most recent three years.

std::string FormatDate(const std::tm& tm) {
  char buf[32] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
  return std::string(buf);
}

// Return the local date key for today minus delta_days.
std::string DateOffset(int delta_days) {
  std::time_t now = std::time(nullptr);
  std::tm tm = {};
  localtime_s(&tm, &now);
  tm.tm_hour = 12;
  tm.tm_mday -= delta_days;
  std::mktime(&tm);
  return FormatDate(tm);
}

// Escape strings used as JSON keys.
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

// Read an unsigned integer field from the line-oriented stats JSON.
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

// ---------------- Singleton ----------------

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

// ---------------- Event collection ----------------

void InputStats::AddKeystroke() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    const std::string today = TodayKey();
    auto& counters = daily_[today];
    ++counters.keystrokes;
    if (has_last_key_down_ && last_key_date_ == today) {
      const auto elapsed = now - last_key_down_;
      if (elapsed > std::chrono::steady_clock::duration::zero() &&
          elapsed <= kMaxInputGap) {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();
        if (milliseconds > 0 &&
            static_cast<unsigned long long>(milliseconds) <=
                (std::numeric_limits<unsigned long long>::max)() -
                    counters.active_milliseconds) {
          counters.active_milliseconds +=
              static_cast<unsigned long long>(milliseconds);
        }
      }
    }
    last_key_down_ = now;
    last_key_date_ = today;
    has_last_key_down_ = true;
    dirty_ = true;
    ++data_generation_;
  }
  MaybeAutoSave();
}

void InputStats::AddChars(unsigned long long n) {
  if (n == 0)
    return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& chars = daily_[TodayKey()].chars;
    chars = n > (std::numeric_limits<unsigned long long>::max)() - chars
                ? (std::numeric_limits<unsigned long long>::max)()
                : chars + n;
    dirty_ = true;
    ++data_generation_;
  }
  MaybeAutoSave();
}

// Continue the all-time chars metric while maintaining a versioned baseline for
// reliable speed samples introduced in this build.
void InputStats::AddSpeedChars(unsigned long long n) {
  if (n == 0)
    return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& speed_chars = daily_[TodayKey()].speed_chars;
    speed_chars =
        n > (std::numeric_limits<unsigned long long>::max)() - speed_chars
            ? (std::numeric_limits<unsigned long long>::max)()
            : speed_chars + n;
    dirty_ = true;
    ++data_generation_;
  }
  MaybeAutoSave();
}

void InputStats::MaybeAutoSave() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dirty_)
      return;
    const std::time_t now = std::time(nullptr);
    if (last_save_time_ != 0 && now >= last_save_time_ &&
        now - last_save_time_ < kAutoSaveIntervalSec)
      return;
  }
  SaveInternal(false);
}

// ---------------- Persistence ----------------

bool InputStats::Load() {
  const std::wstring path = StatsFilePathW();
  if (path.empty())
    return false;
  std::ifstream ifs(path.c_str(), std::ios::binary);
  if (!ifs.is_open())
    return true;  // Missing file is a normal empty state.

  std::ostringstream ss;
  ss << ifs.rdbuf();
  const std::string text = ss.str();
  std::map<std::string, StatsCounters> loaded_daily;

  // Optional fields preserve compatibility with older stats.json files.
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const size_t q1 = line.find('"');
    if (q1 == std::string::npos)
      continue;
    const size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos)
      continue;
    const std::string key = line.substr(q1 + 1, q2 - q1 - 1);
    if (!(key.size() == 10 && key[4] == '-' && key[7] == '-'))
      continue;
    StatsCounters counters;
    unsigned long long value = 0;
    if (ExtractNumber(line, q2, "keystrokes", &value))
      counters.keystrokes = value;
    if (ExtractNumber(line, q2, "chars", &value))
      counters.chars = value;
    if (ExtractNumber(line, q2, "speed_chars", &value))
      counters.speed_chars = value;
    if (ExtractNumber(line, q2, "active_milliseconds", &value))
      counters.active_milliseconds = value;
    loaded_daily[key] = counters;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    daily_.swap(loaded_daily);
    has_last_key_down_ = false;
    last_key_date_.clear();
    dirty_ = false;
    ++data_generation_;
  }
  return true;
}

bool InputStats::Save() { return SaveInternal(true); }

bool InputStats::SaveInternal(bool force) {
  const std::wstring path = StatsFilePathW();
  if (path.empty())
    return false;

  std::lock_guard<std::mutex> save_lock(save_mutex_);
  std::map<std::string, StatsCounters> snapshot;
  unsigned long long snapshot_generation = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::time_t now = std::time(nullptr);
    if (!force) {
      if (!dirty_)
        return true;
      if (last_save_time_ != 0 && now >= last_save_time_ &&
          now - last_save_time_ < kAutoSaveIntervalSec)
        return true;
    }

    bool pruned = false;
    if (!daily_.empty()) {
      const std::string cutoff = DateOffset(kKeepYears * 365 + 1);
      for (auto it = daily_.begin(); it != daily_.end();) {
        if (it->first < cutoff) {
          it = daily_.erase(it);
          pruned = true;
        } else {
          ++it;
        }
      }
    }
    if (pruned)
      ++data_generation_;
    snapshot = daily_;
    snapshot_generation = data_generation_;
  }

  std::ostringstream oss;
  oss << "{\n  \"version\": 1,\n  \"daily\": {\n";
  bool first = true;
  for (const auto& entry : snapshot) {
    if (!first)
      oss << ",\n";
    first = false;
    const StatsCounters& counters = entry.second;
    oss << "    \"" << JsonEscape(entry.first)
        << "\": { \"keystrokes\": " << counters.keystrokes
        << ", \"chars\": " << counters.chars
        << ", \"speed_chars\": " << counters.speed_chars
        << ", \"active_milliseconds\": "
        << counters.active_milliseconds << " }";
  }
  oss << "\n  }\n}\n";
  const std::string content = oss.str();

  const std::wstring tmp = path + L".tmp";
  {
    std::ofstream ofs(tmp.c_str(), std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
      LOG(ERROR) << "InputStats: cannot write tmp file";
      return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    if (!ofs) {
      LOG(ERROR) << "InputStats: failed writing tmp file";
      return false;
    }
  }
  if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    LOG(ERROR) << "InputStats: rename tmp failed";
    return false;
  }

  // Clear dirty only if no newer statistics arrived during this save.
  {
    std::lock_guard<std::mutex> lock(mutex_);
    last_save_time_ = std::time(nullptr);
    if (data_generation_ == snapshot_generation)
      dirty_ = false;
  }
  return true;
}

// ---------------- Aggregation ----------------

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
