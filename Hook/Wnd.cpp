// Wnd.cpp 负责 MapleStory 客户端窗口相关的功能扩展。
// 主要用于修复窗口样式，使部分版本下窗口模式能显示最小化按钮。
//
// 主要功能：
// 1. FixMinimizeButton：修正窗口样式，强制显示最小化按钮。
#include "Wnd.h"

#include "Resources/AOBList.h"
#include "Share/Tool.h"

namespace Wnd {
	// 修正窗口样式，强制显示最小化按钮（适用于 CMS79-CMS84 窗口模式）
	void FixMinimizeButton(DWORD& dwStyle) {
		// CMS79-CMS84 don't show the minimize button when using window mode
		dwStyle |= WS_MINIMIZEBOX;
	}
}