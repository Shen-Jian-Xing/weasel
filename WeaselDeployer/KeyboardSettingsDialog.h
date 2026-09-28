#pragma once

#include "resource.h"
#include <rime_levers_api.h>
#include "KeyboardSettings.h"

class KeyboardSettingsDialog : public CDialogImpl<KeyboardSettingsDialog> {
 public:
  enum { IDD = IDD_KEYBOARD_SETTING };

  explicit KeyboardSettingsDialog(KeyboardSettings* settings);
  ~KeyboardSettingsDialog();

 protected:
  BEGIN_MSG_MAP(KeyboardSettingsDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);

  void Populate();

 private:
  KeyboardSettings* settings_;
  bool loaded_;

  // 中英切换修饰键
  CComboBox shift_l_;
  CComboBox shift_r_;
  CComboBox ctrl_l_;
  CComboBox ctrl_r_;
  CComboBox caps_lock_;

  // 功能快捷键
  CComboBox fn_script_;
  CComboBox fn_ascii_;
  CComboBox fn_shape_;
  CComboBox fn_schema_;

  // 默认字形（单选）
  CButton script_hans_;
  CButton script_hant_;
};
