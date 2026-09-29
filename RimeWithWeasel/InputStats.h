#pragma once

#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace weasel {

// Single-day or aggregated input statistics.
struct StatsCounters {
  unsigned long long keystrokes = 0;  // 击键次数
  unsigned long long chars = 0;       // 上屏字符数（UTF-8 码点）
  unsigned long long speed_chars = 0;  // 新版本采集的速度分子
  unsigned long long active_milliseconds = 0;  // 有效输入时间（毫秒）

  StatsCounters& operator+=(const StatsCounters& other) {
    keystrokes += other.keystrokes;
    chars += other.chars;
    active_milliseconds += other.active_milliseconds;
    speed_chars += other.speed_chars;
    return *this;
  }
};

// 「输入统计」累加器（单例）。仅保存按日聚合数据，不保存输入内容；
// 共享状态由 mutex_ 保护，文件写入由 save_mutex_ 串行化。
class InputStats {
 public:
  static InputStats& Instance();

  // ---- 采集（埋点调用）----
  void AddKeystroke();  // 击键 +1，并累计有效输入时间
  void AddChars(unsigned long long n);  // 上屏字符 +n
  void AddSpeedChars(unsigned long long n);  // 为速度统计累加上屏字符

  // ---- 持久化 ----
  bool Load();  // 启动时读取，兼容缺少新增字段的旧数据
  bool Save();  // 原子写回 stats.json
  // 采集时距上次保存超过 30 秒且有改动时，自动写入 stats.json
  void MaybeAutoSave();

  // ---- Aggregated queries ----
  StatsCounters Today() const;
  // 近 n 天（含今天），按日期倒序；无数据的日期补 0
  std::vector<std::pair<std::string, StatsCounters>> RecentDays(int n) const;
  // 按月聚合，键为 YYYY-MM，按时间倒序
  std::vector<std::pair<std::string, StatsCounters>> ByMonth() const;
  // 按年聚合，键为 YYYY，按时间倒序
  std::vector<std::pair<std::string, StatsCounters>> ByYear() const;
  StatsCounters Total() const;

  static std::wstring StatsFilePathW();
  static std::string StatsFilePath();

 private:
  InputStats() = default;
  InputStats(const InputStats&) = delete;
  InputStats& operator=(const InputStats&) = delete;
  bool SaveInternal(bool force);

  static std::string TodayKey();
  static constexpr int kAutoSaveIntervalSec = 30;
  static constexpr auto kMaxInputGap = std::chrono::seconds(5);

  mutable std::mutex mutex_;
  std::mutex save_mutex_;  // Serialize atomic file replacement
  std::map<std::string, StatsCounters> daily_;
  std::chrono::steady_clock::time_point last_key_down_{};
  std::string last_key_date_;
  bool has_last_key_down_ = false;
  std::time_t last_save_time_ = 0;
  unsigned long long data_generation_ = 0;
  bool dirty_ = false;
};

}  // namespace weasel
