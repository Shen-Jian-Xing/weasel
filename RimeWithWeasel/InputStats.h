#pragma once

#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace weasel {

// 单日/区间的统计计数
struct StatsCounters {
  unsigned long long keystrokes = 0;  // 击键次数
  unsigned long long chars = 0;       // 上屏字符数（UTF-8 码点）

  StatsCounters& operator+=(const StatsCounters& other) {
    keystrokes += other.keystrokes;
    chars += other.chars;
    return *this;
  }
};

// 「输入统计」累加器（单例）。
//
// 设计：
//  - 内存中按天累加（daily_，键 "YYYY-MM-DD"）；
//  - 所有展示维度（今天/近7天/按月/按年/总计）均由 daily_ 实时聚合得出；
//  - 落盘为极简 JSON（stats.json），由 Server 单点写、Deployer 只读。
//
// 线程说明：所有方法均带内部锁，可在任意线程调用。
class InputStats {
 public:
  static InputStats& Instance();

  // ---- 采集（埋点调用）----
  void AddKeystroke();               // 击键 +1
  void AddChars(unsigned long long n);  // 上屏字符 +n

  // ---- 持久化 ----
  bool Load();  // 启动时从 stats.json 读取
  bool Save();  // 写回 stats.json（原子：临时文件 + 改名）
  // 若距上次落盘超过 kAutoSaveIntervalSec 且有改动，则自动 Save。
  // 由采集函数内部调用，实现"变化即节流落盘"，无需外部定时器。
  void MaybeAutoSave();

  // ---- 聚合查询 ----
  StatsCounters Today() const;
  // 近 n 天（含今天），键为 "YYYY-MM-DD"，按日期倒序；无数据的日期补 0
  std::vector<std::pair<std::string, StatsCounters>> RecentDays(int n) const;
  // 按月聚合，键 "YYYY-MM"，按时间倒序
  std::vector<std::pair<std::string, StatsCounters>> ByMonth() const;
  // 按年聚合，键 "YYYY"，按时间倒序
  std::vector<std::pair<std::string, StatsCounters>> ByYear() const;
  StatsCounters Total() const;

  // 文件路径
  static std::wstring StatsFilePathW();  // 宽字符路径（供文件操作）
  static std::string StatsFilePath();    // UTF-8 路径（供显示/日志）

 private:
  InputStats() = default;
  InputStats(const InputStats&) = delete;
  InputStats& operator=(const InputStats&) = delete;

  // 当日 yyyy-MM-dd
  static std::string TodayKey();

  static constexpr int kAutoSaveIntervalSec = 30;  // 节流落盘间隔

  mutable std::mutex mutex_;
  std::map<std::string, StatsCounters> daily_;  // key = "YYYY-MM-DD"
  std::time_t last_save_time_ = 0;              // 上次落盘时间
  bool dirty_ = false;                          // 是否有未落盘改动
};

}  // namespace weasel
