#include "stdafx.h"
#include "KeyboardSettingsDialog.h"
#include "KeyboardSettings.h"
#include <WeaselUtility.h>

// 中英切换样式显示文本（三语言）。索引与 kSwitchStyles 一一对应：
// 0:noop 1:inline_ascii 2:commit_text 3:commit_code 4:clear
static const wchar_t* const kLabelsHans[] = {
    L"无", L"临时英文", L"切换中英·上屏", L"切换中英·上屏编码", L"清除"};
static const wchar_t* const kLabelsHant[] = {
    L"無", L"臨時英文", L"切換中英·上屏", L"切換中英·上屏編碼", L"清除"};
static const wchar_t* const kLabelsEng[] = {
    L"None", L"Temp. English", L"Toggle C/E (commit)",
    L"Toggle C/E (commit code)", L"Clear"};

// 功能键可选键值（index 0 = 保持默认，不追加）
static const char* const kHotkeyValues[] = {
    "",       "F9",      "F10",     "F11",        "F12",
    "Control+Shift+1", "Control+Shift+2", "Control+Shift+3",
    "Control+Shift+4", "Control+Alt+1",   "Control+Alt+2",
    "Control+Alt+3",   "Control+Alt+4"};
static const int kHotkeyValueCount =
    sizeof(kHotkeyValues) / sizeof(kHotkeyValues[0]);

static const wchar_t* const kHotkeyDefaultHans = L"保持默认";
static const wchar_t* const kHotkeyDefaultHant = L"保持預設";
static const wchar_t* const kHotkeyDefaultEng = L"Keep default";

static bool IsEnglish() {
  return get_language_id() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}
static bool IsTraditional() {
  return get_language_id() ==
         MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
}

static const wchar_t* const* GetStyleLabels() {
  if (IsEnglish())
    return kLabelsEng;
  if (IsTraditional())
    return kLabelsHant;
  return kLabelsHans;
}

// 把 style 字符串映射到下拉框索引；未知/空 → 0 (noop)
static int StyleToIndex(const std::string& style) {
  if (style.empty())
    return 0;
  for (int i = 0; i < kSwitchStyleCount; ++i) {
    if (style == kSwitchStyles[i])
      return i;
  }
  return 0;
}

// 功能键值 -> 下拉框索引（未找到 -> 0）
static int HotkeyToIndex(const std::string& value) {
  for (int i = 0; i < kHotkeyValueCount; ++i) {
    if (value == kHotkeyValues[i])
      return i;
  }
  return 0;
}

KeyboardSettingsDialog::KeyboardSettingsDialog(KeyboardSettings* settings)
    : settings_(settings), loaded_(false) {}

KeyboardSettingsDialog::~KeyboardSettingsDialog() {}

void KeyboardSettingsDialog::Populate() {
  if (!settings_)
    return;

  // ---- 中英切换修饰键 ----
  const wchar_t* const* labels = GetStyleLabels();
  CComboBox* boxes[] = {&shift_l_, &shift_r_, &ctrl_l_, &ctrl_r_, &caps_lock_};
  for (int b = 0; b < 5; ++b) {
    CComboBox* box = boxes[b];
    box->ResetContent();
    for (int i = 0; i < kSwitchStyleCount; ++i)
      box->AddString(labels[i]);
    std::string style = settings_->GetSwitchKeyStyle(kSwitchKeys[b]);
    box->SetCurSel(StyleToIndex(style));
  }

  // ---- 功能快捷键 ----
  const wchar_t* default_label = IsEnglish()
                                     ? kHotkeyDefaultEng
                                     : (IsTraditional() ? kHotkeyDefaultHant
                                                        : kHotkeyDefaultHans);
  CComboBox* fn_boxes[] = {&fn_script_, &fn_ascii_, &fn_shape_, &fn_schema_};
  for (int f = 0; f < kFunctionHotkeyCount; ++f) {
    CComboBox* box = fn_boxes[f];
    box->ResetContent();
    box->AddString(default_label);
    for (int i = 1; i < kHotkeyValueCount; ++i) {
      // 把 UTF-8 键名转成宽字符显示
      wchar_t wbuf[64] = {0};
      MultiByteToWideChar(CP_UTF8, 0, kHotkeyValues[i], -1, wbuf, 63);
      box->AddString(wbuf);
    }
    std::string current = settings_->GetFunctionHotkey(f);
    box->SetCurSel(HotkeyToIndex(current));
  }

  // ---- 默认字形 ----
  bool hant = settings_->GetDefaultScript();
  script_hant_.SetCheck(hant ? BST_CHECKED : BST_UNCHECKED);
  script_hans_.SetCheck(hant ? BST_UNCHECKED : BST_CHECKED);

  loaded_ = true;
}

LRESULT KeyboardSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  shift_l_.Attach(GetDlgItem(IDC_KB_SHIFT_L));
  shift_r_.Attach(GetDlgItem(IDC_KB_SHIFT_R));
  ctrl_l_.Attach(GetDlgItem(IDC_KB_CTRL_L));
  ctrl_r_.Attach(GetDlgItem(IDC_KB_CTRL_R));
  caps_lock_.Attach(GetDlgItem(IDC_KB_CAPS_LOCK));

  fn_script_.Attach(GetDlgItem(IDC_KB_FN_SCRIPT));
  fn_ascii_.Attach(GetDlgItem(IDC_KB_FN_ASCII));
  fn_shape_.Attach(GetDlgItem(IDC_KB_FN_SHAPE));
  fn_schema_.Attach(GetDlgItem(IDC_KB_FN_SCHEMA));

  script_hans_.Attach(GetDlgItem(IDC_KB_SCRIPT_HANS));
  script_hant_.Attach(GetDlgItem(IDC_KB_SCRIPT_HANT));

  Populate();

  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT KeyboardSettingsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT KeyboardSettingsDialog::OnOK(WORD, WORD code, HWND, BOOL&) {
  if (loaded_ && settings_) {
    // ---- 中英切换修饰键 ----
    CComboBox* boxes[] = {&shift_l_, &shift_r_, &ctrl_l_, &ctrl_r_, &caps_lock_};
    for (int b = 0; b < 5; ++b) {
      int idx = boxes[b]->GetCurSel();
      if (idx < 0 || idx >= kSwitchStyleCount)
        idx = 0;
      settings_->SetSwitchKey(kSwitchKeys[b], kSwitchStyles[idx]);
    }

    // ---- 功能快捷键（追加式；仅当与已记录值不同时追加）----
    CComboBox* fn_boxes[] = {&fn_script_, &fn_ascii_, &fn_shape_, &fn_schema_};
    for (int f = 0; f < kFunctionHotkeyCount; ++f) {
      int idx = fn_boxes[f]->GetCurSel();
      if (idx < 0 || idx >= kHotkeyValueCount)
        idx = 0;
      std::string selected = kHotkeyValues[idx];
      std::string recorded = settings_->GetFunctionHotkey(f);
      if (!selected.empty() && selected != recorded)
        settings_->AppendFunctionHotkey(f, selected);
    }

    // ---- 默认字形 ----
    bool hant = (script_hant_.GetCheck() == BST_CHECKED);
    settings_->SetDefaultScript(hant);
  }
  EndDialog(code);
  return 0;
}
