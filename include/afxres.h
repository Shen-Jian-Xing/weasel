// afxres.h — 轻量替代头文件
//
// 小狼毫的 WeaselTSF.rc / WeaselServer.rc 通过 #include "afxres.h" 引用
// MFC 资源常量（如 IDC_STATIC 等）。本文件在未安装 MFC 组件的情况下，
// 转发到 Windows SDK 官方自带的 winres.h（MFC 资源定义的无依赖版本），
// 从而无需安装完整的 MFC（约 1~2GB）即可编译资源。
//
// 说明：这两个 .rc 仅使用资源常量，不使用任何 MFC 运行时/类，
//       因此该替代与安装 MFC 时对资源编译而言行为一致。
#pragma once

#include <winres.h>
