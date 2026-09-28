#include "stdafx.h"
#include "StatsDialog.h"
#include <WeaselUtility.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {

struct Counters {
  unsigned long long keystrokes = 0;
  unsigned long long chars = 0;
};

bool ExtractNum(const std::string& line,
                const std::string& key,
                unsigned long long* out) {
  std::string pat = "\"" + key + "\"";
  size_t p = line.find(pat);
  if (p == std::string::npos)
    return false;
  p = line.find(':', p + pat.size());
  if (p == std::string::npos)
    return false;
  ++p;
  while (p < line.size() && (line[p] < '0' || line[p] > '9'))
    ++p;
  size_t q = p;
  while (q < line.size() && line[q] >= '0' && line[q] <= '9')
    ++q;
  if (q == p)
    return false;
  *out = std::strtoull(line.substr(p, q - p).c_str(), nullptr, 10);
  return true;
}

std::map<std::string, Counters> ReadDaily() {
  std::map<std::string, Counters> daily;
  try {
    fs::path p = WeaselUserDataPath() / L"stats.json";
    std::ifstream ifs(p.wstring().c_str(), std::ios::binary);
    if (!ifs.is_open())
      return daily;
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string line;
    while (std::getline(ss, line)) {
      size_t q1 = line.find('"');
      if (q1 == std::string::npos)
        continue;
      size_t q2 = line.find('"', q1 + 1);
      if (q2 == std::string::npos)
        continue;
      std::string key = line.substr(q1 + 1, q2 - q1 - 1);
      if (!(key.size() == 10 && key[4] == '-' && key[7] == '-'))
        continue;
      Counters c;
      ExtractNum(line, "keystrokes", &c.keystrokes);
      ExtractNum(line, "chars", &c.chars);
      daily[key] = c;
    }
  } catch (...) {
  }
  return daily;
}

std::string DateOffset(int delta) {
  std::time_t now = std::time(nullptr);
  std::tm tm = {};
  localtime_s(&tm, &now);
  tm.tm_hour = 12;
  tm.tm_mday -= delta;
  std::mktime(&tm);
  char buf[32] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
  return std::string(buf);
}

std::string TodayKey() { return DateOffset(0); }

std::string Thousands(unsigned long long n) {
  std::string s = std::to_string(n);
  std::string out;
  int cnt = 0;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    if (cnt > 0 && cnt % 3 == 0)
      out += ',';
    out += *it;
    ++cnt;
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::vector<std::pair<std::string, Counters>> Aggregate(
    const std::map<std::string, Counters>& daily,
    int prefix_len) {
  std::map<std::string, Counters> agg;
  for (const auto& kv : daily) {
    if (static_cast<int>(kv.first.size()) >= prefix_len) {
      auto& c = agg[kv.first.substr(0, prefix_len)];
      c.keystrokes += kv.second.keystrokes;
      c.chars += kv.second.chars;
    }
  }
  std::vector<std::pair<std::string, Counters>> result(agg.begin(), agg.end());
  std::reverse(result.begin(), result.end());
  return result;
}

bool IsEnglish() {
  return get_language_id() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}
bool IsTraditional() {
  return get_language_id() ==
         MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
}

struct Labels {
  const wchar_t* title;
  const wchar_t* privacy;
  const wchar_t* today;
  const wchar_t* total;
  const wchar_t* recent7;
  const wchar_t* keystrokes;
  const wchar_t* chars_unit;
  const wchar_t* month;
  const wchar_t* year;
  const wchar_t* close;
  const wchar_t* open_file;
};

Labels GetLabels() {
  if (IsEnglish()) {
    return {L"[Weasel] Input Statistics",
            L"Keystrokes and committed characters only; no input content is stored",
            L"Today", L"All time", L"Last 7 days", L"keystrokes", L"chars",
            L"Month", L"Year", L"Close", L"Open data file"};
  }
  if (IsTraditional()) {
    return {L"[小狼毫] 輸入統計",
            L"僅統計擊鍵與上屏字符，不記錄輸入內容",
            L"今天", L"累計", L"近 7 天", L"擊鍵", L"字",
            L"按月", L"按年", L"關閉", L"打開數據文件"};
  }
  return {L"[小狼毫] 输入统计",
          L"仅统计击键与上屏字符，不记录输入内容",
          L"今天", L"累计", L"近 7 天", L"击键", L"字",
          L"按月", L"按年", L"关闭", L"打开数据文件"};
}

COLORREF Rgb(BYTE r, BYTE g, BYTE b) { return RGB(r, g, b); }

void Fill(HDC dc, const RECT& rc, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rc, brush);
  DeleteObject(brush);
}

void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HGDIOBJ old = SelectObject(dc, pen);
  MoveToEx(dc, x1, y1, nullptr);
  LineTo(dc, x2, y2);
  SelectObject(dc, old);
  DeleteObject(pen);
}

void Text(HDC dc,
          const wchar_t* value,
          RECT rc,
          COLORREF color,
          HFONT font,
          UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
  SetTextColor(dc, color);
  HGDIOBJ old = SelectObject(dc, font);
  DrawTextW(dc, value, -1, &rc, flags | DT_NOPREFIX);
  SelectObject(dc, old);
}

HFONT MakeFont(int height, int weight, const wchar_t* face) {
  return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

std::wstring Number(unsigned long long value) {
  return u8tow(Thousands(value));
}

}  // namespace

StatsDialog::StatsDialog() {}
StatsDialog::~StatsDialog() {}

LRESULT StatsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  const Labels labels = GetLabels();
  SetWindowTextW(labels.title);
  ::SetDlgItemTextW(m_hWnd, IDC_STATS_OPEN_FILE, labels.open_file);
  ::SetDlgItemTextW(m_hWnd, IDOK, labels.close);
  ::InvalidateRect(m_hWnd, nullptr, TRUE);
  CenterWindow();
  return TRUE;
}

LRESULT StatsDialog::OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&) {
  return TRUE;
}

LRESULT StatsDialog::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
  PAINTSTRUCT ps = {};
  HDC dc = ::BeginPaint(m_hWnd, &ps);
  RECT client = {};
  ::GetClientRect(m_hWnd, &client);
  DrawDashboard(dc, client);
  ::EndPaint(m_hWnd, &ps);
  return 0;
}

void StatsDialog::DrawDashboard(HDC dc, const RECT& client) {
  const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
  const auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };
  const int width = client.right - client.left;
  const int margin = scale(22);
  const int inner_width = std::max(1, width - margin * 2);
  const Labels labels = GetLabels();
  const auto daily = ReadDaily();

  const COLORREF bg = Rgb(255, 255, 255);
  const COLORREF primary = Rgb(40, 94, 174);
  const COLORREF text = Rgb(36, 36, 36);
  const COLORREF secondary = Rgb(104, 104, 104);
  const COLORREF muted = Rgb(132, 132, 132);
  const COLORREF rule = Rgb(231, 231, 231);
  const COLORREF bar = Rgb(143, 172, 219);
  const COLORREF today_bar = Rgb(53, 106, 195);

  Fill(dc, client, bg);
  SetBkMode(dc, TRANSPARENT);

  HFONT font_normal = MakeFont(scale(12), FW_NORMAL, L"Microsoft YaHei UI");
  HFONT font_small = MakeFont(scale(10), FW_NORMAL, L"Microsoft YaHei UI");
  HFONT font_medium = MakeFont(scale(12), FW_SEMIBOLD, L"Microsoft YaHei UI");
  HFONT font_large = MakeFont(scale(27), FW_SEMIBOLD, L"Microsoft YaHei UI");

  RECT rc = {margin, scale(13), width - margin, scale(34)};
  Text(dc, labels.privacy, rc, muted, font_small);

  // Two compact summary cells: today and all-time.
  const int summary_top = scale(42);
  const int summary_height = scale(82);
  RECT summary = {margin, summary_top, width - margin,
                  summary_top + summary_height};
  HPEN border = CreatePen(PS_SOLID, 1, rule);
  HGDIOBJ old_pen = SelectObject(dc, border);
  HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
  Rectangle(dc, summary.left, summary.top, summary.right, summary.bottom);
  SelectObject(dc, old_brush);
  SelectObject(dc, old_pen);
  DeleteObject(border);
  Line(dc, width / 2, summary.top, width / 2, summary.bottom, rule);

  Counters today;
  auto today_it = daily.find(TodayKey());
  if (today_it != daily.end())
    today = today_it->second;
  Counters total;
  for (const auto& kv : daily) {
    total.keystrokes += kv.second.keystrokes;
    total.chars += kv.second.chars;
  }

  const int cell_padding = scale(15);
  const int left_value = summary.left + cell_padding;
  const int right_value = width / 2 + cell_padding;
  rc = {left_value, summary.top + scale(9), width / 2 - scale(8),
        summary.top + scale(29)};
  Text(dc, labels.today, rc, secondary, font_normal);
  rc = {right_value, summary.top + scale(9), summary.right - cell_padding,
        summary.top + scale(29)};
  Text(dc, labels.total, rc, secondary, font_normal);

  std::wstring number = Number(today.keystrokes);
  rc = {left_value, summary.top + scale(29), width / 2 - scale(8),
        summary.top + scale(61)};
  Text(dc, number.c_str(), rc, text, font_large);
  number = Number(total.keystrokes);
  rc = {right_value, summary.top + scale(29), summary.right - cell_padding,
        summary.top + scale(61)};
  Text(dc, number.c_str(), rc, text, font_large);

  std::wstring detail = std::wstring(labels.chars_unit) + L"  " + Number(today.chars);
  rc = {left_value, summary.top + scale(61), width / 2 - scale(8),
        summary.bottom - scale(3)};
  Text(dc, detail.c_str(), rc, secondary, font_small);
  detail = std::wstring(labels.chars_unit) + L"  " + Number(total.chars);
  rc = {right_value, summary.top + scale(61), summary.right - cell_padding,
        summary.bottom - scale(3)};
  Text(dc, detail.c_str(), rc, secondary, font_small);

  // Seven-day compact bar chart.
  const int section_top = scale(145);
  rc = {margin, section_top, width - margin, section_top + scale(21)};
  Text(dc, labels.recent7, rc, text, font_medium);
  RECT unit_rc = {width - margin - scale(95), section_top,
                  width - margin, section_top + scale(21)};
  Text(dc, labels.keystrokes, unit_rc, muted, font_small,
       DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

  const int chart_top = section_top + scale(27);
  const int bar_area_height = scale(61);
  const int labels_top = chart_top + bar_area_height + scale(4);
  const int chart_bottom = chart_top + bar_area_height;
  std::vector<Counters> last7(7);
  std::vector<std::string> dates(7);
  unsigned long long max_keystrokes = 0;
  for (int i = 0; i < 7; ++i) {
    dates[i] = DateOffset(6 - i);
    auto it = daily.find(dates[i]);
    if (it != daily.end())
      last7[i] = it->second;
    max_keystrokes = std::max(max_keystrokes, last7[i].keystrokes);
  }

  const int slot = inner_width / 7;
  const int bar_width = std::min(scale(18), std::max(scale(8), slot / 3));
  for (int i = 0; i < 7; ++i) {
    int bar_height = 0;
    if (max_keystrokes > 0 && last7[i].keystrokes > 0) {
      bar_height = static_cast<int>(
          (static_cast<double>(last7[i].keystrokes) / max_keystrokes) *
          (bar_area_height - scale(5)));
      bar_height = std::max(scale(3), bar_height);
    }
    const int center = margin + slot * i + slot / 2;
    if (bar_height > 0) {
      RECT bar_rc = {center - bar_width / 2, chart_bottom - bar_height,
                     center + (bar_width + 1) / 2, chart_bottom};
      Fill(dc, bar_rc, i == 6 ? today_bar : bar);
    }
    std::wstring day_label;
    if (i == 6) {
      day_label = IsEnglish() ? L"Today" : L"今天";
    } else {
      day_label = u8tow(dates[i].substr(5));
    }
    RECT day_rc = {margin + slot * i, labels_top,
                   margin + slot * (i + 1), labels_top + scale(17)};
    Text(dc, day_label.c_str(), day_rc, i == 6 ? primary : muted, font_small,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  Line(dc, margin, chart_bottom, width - margin, chart_bottom, rule);

  // Current month and year summaries, each on one quiet row.
  const int period_top = labels_top + scale(27);
  Line(dc, margin, period_top, width - margin, period_top, rule);
  const auto month = Aggregate(daily, 7);
  const auto year = Aggregate(daily, 4);
  auto draw_period = [&](int y, const wchar_t* caption,
                         const std::string& period_key,
                         const Counters& counters) {
    RECT name_rc = {margin, y, margin + scale(70), y + scale(31)};
    Text(dc, caption, name_rc, secondary, font_normal);
    RECT date_rc = {margin + scale(70), y, margin + scale(158), y + scale(31)};
    std::wstring date = u8tow(period_key);
    Text(dc, date.c_str(), date_rc, text, font_normal);
    std::wstring summary_text = Number(counters.keystrokes) + L" " +
                                labels.keystrokes + L"   ·   " +
                                Number(counters.chars) + L" " + labels.chars_unit;
    RECT data_rc = {margin + scale(158), y, width - margin, y + scale(31)};
    Text(dc, summary_text.c_str(), data_rc, secondary, font_small,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    Line(dc, margin, y + scale(31), width - margin, y + scale(31), rule);
  };

  std::string month_key = TodayKey().substr(0, 7);
  std::string year_key = TodayKey().substr(0, 4);
  Counters month_total, year_total;
  for (const auto& item : month) {
    if (item.first == month_key) {
      month_total = item.second;
      break;
    }
  }
  for (const auto& item : year) {
    if (item.first == year_key) {
      year_total = item.second;
      break;
    }
  }
  draw_period(period_top + scale(1), labels.month, month_key, month_total);
  draw_period(period_top + scale(34), labels.year, year_key, year_total);

  DeleteObject(font_normal);
  DeleteObject(font_small);
  DeleteObject(font_medium);
  DeleteObject(font_large);
}

LRESULT StatsDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT StatsDialog::OnCommandClose(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT StatsDialog::OnOpenFile(WORD, WORD, HWND, BOOL&) {
  try {
    fs::path p = WeaselUserDataPath() / L"stats.json";
    std::wstring quoted = L"\"" + p.wstring() + L"\"";
    if (!fs::exists(p)) {
      MessageBox(quoted.c_str(), L"stats.json", MB_OK | MB_ICONINFORMATION);
      return 0;
    }
    ShellExecuteW(NULL, L"open", quoted.c_str(), NULL, NULL, SW_SHOWNORMAL);
  } catch (...) {
  }
  return 0;
}
