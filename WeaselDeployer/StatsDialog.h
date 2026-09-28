#pragma once

#include "resource.h"

// Native, compact input-statistics dashboard.
class StatsDialog : public CDialogImpl<StatsDialog> {
 public:
  enum { IDD = IDD_STATS };

  StatsDialog();
  ~StatsDialog();

 protected:
  BEGIN_MSG_MAP(StatsDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_PAINT, OnPaint)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
  COMMAND_ID_HANDLER(IDOK, OnCommandClose)
  COMMAND_ID_HANDLER(IDCANCEL, OnCommandClose)
  COMMAND_ID_HANDLER(IDC_STATS_OPEN_FILE, OnOpenFile)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnWindowClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCommandClose(WORD, WORD, HWND, BOOL&);
  LRESULT OnOpenFile(WORD, WORD, HWND, BOOL&);

  void DrawDashboard(HDC dc, const RECT& client);
};
