#pragma once

#include <string>
#include <vector>
#include <rime_levers_api.h>

// 中英切换修饰键的固定键列表（与 librime ascii_composer 支持的键一致）
// 顺序即 UI 中下拉框的顺序。
extern const char* const kSwitchKeys[];  // {"Shift_L","Shift_R","Control_L","Control_R","Caps_Lock"}
extern const int kSwitchKeyCount;

// 可选样式（switch_key 的值），顺序与 UI 下拉框一致。
extern const char* const kSwitchStyles[];  // {"noop","inline_ascii","commit_text","commit_code","clear"}
extern const int kSwitchStyleCount;

// 4 个功能快捷键的固定动作定义（顺序即 UI 下拉框顺序）。
// action: swich_key 字段名（toggle/select 的值）
// kind:   "toggle" 或 "select"
extern const char* const kFunctionHotkeys[];  // {"simplification","ascii_mode","full_shape",".next"}
extern const char* const kFunctionKinds[];    // {"toggle","toggle","toggle","select"}
extern const int kFunctionHotkeyCount;

// 管理「快捷键设定」的各项配置读写：
//  - 中英切换修饰键   -> default.custom.yaml 的 ascii_composer/switch_key
//  - 功能快捷键       -> default.custom.yaml 的 key_binder/bindings/+（追加）
//  - 默认字形（简/繁）-> user.yaml 的 var/option/zh_hans / zh_hant
class KeyboardSettings {
 public:
  KeyboardSettings();

  // ---- 中英切换修饰键 ----
  // 读取当前配置值（未设置时返回空串）
  std::string GetSwitchKeyStyle(const std::string& key);
  // 设置某键的切换样式；key 属于 kSwitchKeys，style 属于 kSwitchStyles
  bool SetSwitchKey(const std::string& key, const std::string& style);

  // ---- 功能快捷键 ----
  // 读取某功能键的当前 accept 键（未设置时返回空串）。
  // index 对应 kFunctionHotkeys。
  std::string GetFunctionHotkey(int index);
  // 追加一条功能键绑定（追加语义，不影响内置）。
  // index 对应 kFunctionHotkeys；accept 形如 "F9" / "Control+Shift+4"。
  bool AppendFunctionHotkey(int index, const std::string& accept);

  // ---- 默认字形 ----
  // 返回 true 表示默认繁体，false 表示默认简体。
  bool GetDefaultScript();
  // hant = true 设默认繁体，false 设默认简体。
  bool SetDefaultScript(bool hant);

  RimeCustomSettings* settings() { return settings_; }

 private:
  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
};
