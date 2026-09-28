#include "stdafx.h"
#include "KeyboardSettings.h"
#include <cstring>

const char* const kSwitchKeys[] = {"Shift_L", "Shift_R", "Control_L", "Control_R",
                                   "Caps_Lock"};
const int kSwitchKeyCount = sizeof(kSwitchKeys) / sizeof(kSwitchKeys[0]);

const char* const kSwitchStyles[] = {"noop", "inline_ascii", "commit_text",
                                     "commit_code", "clear"};
const int kSwitchStyleCount = sizeof(kSwitchStyles) / sizeof(kSwitchStyles[0]);

// 4 个功能键：动作 + 类型（对应 UI 顺序）
const char* const kFunctionHotkeys[] = {"zh_hans", "ascii_mode", "full_shape",
                                        ".next"};
const char* const kFunctionKinds[] = {"toggle", "toggle", "toggle", "select"};
const int kFunctionHotkeyCount =
    sizeof(kFunctionHotkeys) / sizeof(kFunctionHotkeys[0]);

// 存于 user.yaml 的功能键记录键名（便于 UI 读回）
static const char* const kFnRecordKeys[] = {"script", "ascii", "shape", "schema"};

// ---------- 构造 / 析构 ----------

KeyboardSettings::KeyboardSettings() {
  api_ = (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
  settings_ = api_->custom_settings_init("default", "Weasel::KeyboardSettings");
}

// ---------- 中英切换修饰键 ----------

std::string KeyboardSettings::GetSwitchKeyStyle(const std::string& key) {
  if (!settings_)
    return std::string();
  RimeConfig config = {0};
  api_->settings_get_config(settings_, &config);
  std::string path = "ascii_composer/switch_key/" + key;
  const char* value = rime_get_api()->config_get_cstring(&config, path.c_str());
  if (!value)
    return std::string();
  return std::string(value);
}

bool KeyboardSettings::SetSwitchKey(const std::string& key,
                                    const std::string& style) {
  if (!settings_)
    return false;
  std::string path = "ascii_composer/switch_key/" + key;
  return api_->customize_string(settings_, path.c_str(), style.c_str()) != 0;
}

// ---------- 功能快捷键 ----------

std::string KeyboardSettings::GetFunctionHotkey(int index) {
  if (index < 0 || index >= kFunctionHotkeyCount)
    return std::string();
  RimeConfig cfg = {0};
  if (!rime_get_api()->user_config_open("user", &cfg))
    return std::string();
  std::string path = "weasel/fn_hotkeys/" + std::string(kFnRecordKeys[index]);
  const char* value = rime_get_api()->config_get_cstring(&cfg, path.c_str());
  std::string result = value ? std::string(value) : std::string();
  rime_get_api()->config_close(&cfg);
  return result;
}

bool KeyboardSettings::AppendFunctionHotkey(int index,
                                            const std::string& accept) {
  if (!settings_ || index < 0 || index >= kFunctionHotkeyCount)
    return false;
  if (accept.empty())
    return false;

  // 1) 构造一条绑定，追加到 key_binder/bindings/+
  std::string yaml = "- {when: always, accept: \"" + accept + "\", ";
  if (std::strcmp(kFunctionKinds[index], "select") == 0)
    yaml += "select: \"" + std::string(kFunctionHotkeys[index]) + "\"}\n";
  else
    yaml += "toggle: " + std::string(kFunctionHotkeys[index]) + "}\n";

  RimeConfig list_cfg = {0};
  if (!rime_get_api()->config_load_string(&list_cfg, yaml.c_str()))
    return false;
  bool ok = api_->customize_item(settings_, "key_binder/bindings/+",
                                 &list_cfg) != 0;
  rime_get_api()->config_close(&list_cfg);
  if (!ok)
    return false;

  // 2) 记录到 user.yaml（供 UI 读回）
  RimeConfig user_cfg = {0};
  if (rime_get_api()->user_config_open("user", &user_cfg)) {
    std::string path =
        "weasel/fn_hotkeys/" + std::string(kFnRecordKeys[index]);
    rime_get_api()->config_set_string(&user_cfg, path.c_str(), accept.c_str());
    rime_get_api()->config_close(&user_cfg);  // auto_save 落盘
  }
  return true;
}

// ---------- 默认字形 ----------

bool KeyboardSettings::GetDefaultScript() {
  RimeConfig cfg = {0};
  if (!rime_get_api()->user_config_open("user", &cfg))
    return false;
  Bool zh_hans = False, zh_hant = False;
  rime_get_api()->config_get_bool(&cfg, "var/option/zh_hans", &zh_hans);
  rime_get_api()->config_get_bool(&cfg, "var/option/zh_hant", &zh_hant);
  rime_get_api()->config_close(&cfg);
  return zh_hant && !zh_hans;
}

bool KeyboardSettings::SetDefaultScript(bool hant) {
  RimeConfig cfg = {0};
  if (!rime_get_api()->user_config_open("user", &cfg))
    return false;
  rime_get_api()->config_set_bool(&cfg, "var/option/zh_hans", Bool(!hant));
  rime_get_api()->config_set_bool(&cfg, "var/option/zh_hant", Bool(hant));
  rime_get_api()->config_close(&cfg);  // auto_save 落盘
  return true;
}
