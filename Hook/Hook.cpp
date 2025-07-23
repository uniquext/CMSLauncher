// Hook.cpp 主要负责 MapleStory 客户端的核心 Hook 逻辑，包括 API 拦截、网络初始化、资源挂载、窗口处理等。
// 通过对关键 Windows API 进行 Hook，实现对游戏客户端的功能扩展和行为修改。
//
// 主要流程：
// 1. 通过 GetStartupInfoA_Hook 完成网络初始化、域名修正、资源挂载等操作。
// 2. 通过 CreateMutexA_Hook 完成网络重定向、数据包 Hook、去除多开检测等。
// 3. 通过 CreateWindowExA_Hook 实现窗口标题自定义、广告窗口拦截等。
// 4. 通过 ImmAssociateContext_Hook 实现输入法支持和伤害字皮肤应用。
// 5. Install/Uninstall 用于安装和卸载所有 Hook。
#include "Hook.h"
#include "Network.h"
#include "ResMan.h"
#include "Wnd.h"
#include "DamageSkin.h"

#include "Resources/Config.h"
#include "Share/Funcs.h"
#include "Share/Tool.h"

#include "HookEx.h"
#pragma comment(lib, "HookEx.lib")

#include <imm.h>
#pragma comment(lib, "imm32.lib")

#include<intrin.h>
#pragma intrinsic(_ReturnAddress)

// 匿名命名空间，存放本文件私有的静态变量和函数
namespace {
	// gMapleR 用于管理 MapleStory 客户端的模块信息和资源操作
	static Rosemary gMapleR;
	// 标记各 Hook 是否已加载，防止重复初始化
	static bool bGetStartupInfoALoaded = false;
	static bool bCreateMutexALoaded = false;
	static bool bImmAssociateContextLoaded = false;

	// 获取当前进程的所有模块信息（用于判断调用者是否为主 EXE）
	void GetModuleEntryList(std::vector<MODULEENTRY32W>& entryList) {
		DWORD pid = GetCurrentProcessId();

		if (!pid) {
			return;
		}

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return;
		}

		MODULEENTRY32W me32;
		memset(&me32, 0, sizeof(me32));
		me32.dwSize = sizeof(me32);
		if (!Module32FirstW(hSnapshot, &me32)) {
			CloseHandle(hSnapshot);
			return;
		}

		entryList.clear();

		do {
			entryList.push_back(me32);
		} while (Module32NextW(hSnapshot, &me32));

		CloseHandle(hSnapshot);
	}

	// 判断调用者是否为主 EXE（用于确保 Hook 只对主程序生效）
	bool IsEXECaller(void* vReturnAddress) {
		if (gMapleR.IsInitialized()) {
			DEBUG(L"gMapleR has already been initialized");
			return true;
		}
		std::vector<MODULEENTRY32W> entryList;
		GetModuleEntryList(entryList);
		if (entryList.empty()) {
			DEBUG(L"EXE hasn't been call yet");
			return false;
		}
		// first module may be exe
		const MODULEENTRY32W& me32 = entryList[0];
		ULONG_PTR returnAddr = (ULONG_PTR)vReturnAddress;
		ULONG_PTR baseAddr = (ULONG_PTR)me32.modBaseAddr;
		ULONG_PTR endAddr = baseAddr + me32.modBaseSize;

		if (returnAddr >= baseAddr && returnAddr < endAddr) {
			gMapleR.Init(&entryList, L"MapleStory.exe");
			if (gMapleR.IsInitialized()) {
				DEBUG(L"gMapleR init ok");
				return true;
			}
			else {
				DEBUG(L"Failed to init gMapleR");
				return false;
			}
		}
		return false;
	}

	// Hook: GetStartupInfoA，客户端启动时调用，适合做初始化
	static auto _GetStartupInfoA = decltype(&GetStartupInfoA)(GetProcAddress(GetModuleHandle(L"KERNEL32"), "GetStartupInfoA"));
	VOID WINAPI GetStartupInfoA_Hook(LPSTARTUPINFOA lpStartupInfo) {
		// 只在首次调用且由主 EXE 调用时执行初始化逻辑
		// 包括网络初始化、域名修正、去除本地校验、资源挂载、扩展资源管理器等
		if (!bGetStartupInfoALoaded && lpStartupInfo && IsEXECaller(_ReturnAddress())) {
			bGetStartupInfoALoaded = true;
			// Click MapleStory.exe
			if (!Network::InitWSAData()) {
				DEBUG(L"Unable to init WSAData");
			}
			if (!Network::FixDomain(gMapleR)) {
				DEBUG(L"Unable to fix domain");
			}
			if (!HookEx::RemoveLocaleCheck(gMapleR)) {
				DEBUG(L"Unable to remove locale check");
			}
			// Click Play button
			// Load AntiCheat
			if (!HookEx::RemoveSecurityClient(gMapleR)) {
				DEBUG(L"Unable to remove SecurityClient");
			}
			// Check if base.wz exists? wz default mode or img mode
			// Check if multiple.wz exists? wz multiple mode or wz default mode
			if (!HookEx::Mount(gMapleR, Network::GetMapleVersion(), &Config::MapleWZKey)) {
				DEBUG(L"Unable to mount ResMan");
			}
			if (!ResMan::Extend(gMapleR)) {
				DEBUG(L"Unable to extend ResMan");
			}
		}
		_GetStartupInfoA(lpStartupInfo);
	}

	// Hook: CreateMutexA，EXE解包后第一个调用的 Windows API，适合做内存修改和多开处理
	static auto _CreateMutexA = decltype(&CreateMutexA)(GetProcAddress(GetModuleHandleW(L"KERNEL32"), "CreateMutexA"));
	HANDLE WINAPI CreateMutexA_Hook(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName) {
		// 只在首次调用时执行网络重定向、数据包 Hook、去除多开检测等
		// 如果检测到多开互斥体名，直接返回伪造句柄实现多开
		if (!bCreateMutexALoaded) {
			bCreateMutexALoaded = true;
			// Select game area
			if (!Network::Redirect(Config::ServerAddr, Config::LoginServerPort)) {
				DEBUG(L"Unable to redirect addr");
			}
			if (!Network::RecvXOR(Config::RecvXOR)) {
				DEBUG(L"Unable to hook recv");
			}
			if (!Network::SendXOR(Config::SendXOR)) {
				DEBUG(L"Unable to hook send");
			}
			// Login UI is loaded
			if (!HookEx::RemoveManipulatePacketCheck(gMapleR)) {
				DEBUG(L"Unable to remove Game Menu Check");
			}
			if (!HookEx::RemoveRenderFrameCheck(gMapleR)) {
				DEBUG(L"Unable to remove Render Frame Check");
			}
			// Select character
			if (!HookEx::RemoveEnterFieldCheck(gMapleR)) {
				DEBUG(L"Unable to remove Enter Field Check");
			}
		}
		if (lpName && strstr(lpName, "WvsClientMtx")) {
			// MultiClient: faking HANDLE is 0xBADF00D(BadFood)
			return (HANDLE)0xBADF00D;
		}
		return _CreateMutexA(lpMutexAttributes, bInitialOwner, lpName);
	}

	// Hook: CreateWindowExA，窗口创建时调用，用于自定义窗口标题、去除广告窗口等
	static auto _CreateWindowExA = decltype(&CreateWindowExA)(GetProcAddress(LoadLibraryW(L"USER32"), "CreateWindowExA"));
	HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
		// 根据窗口类名判断是否为广告窗口或主窗口，进行拦截或自定义
		if (!Config::IsStartUpDlgSkipped) {
			return _CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
		}
		if (lpClassName && strstr(lpClassName, "StartUpDlgClass")) {
			// Found in ShowADBalloon 
			// AOB C7 45 FC 09 00 00 00 E8 ?? ?? FF FF
			// CMS88(00A1279A)
			// Remove StartUp Ads
			return NULL;
		}
		if (lpClassName && strstr(lpClassName, "MapleStoryClass")) {
			// Found in ShowStartUpWnd
			lpWindowName = Config::WindowTitle.c_str(); //Customize game window title 			
			Wnd::FixMinimizeButton(dwStyle); // Show minimize button for CMS79 - CMS84
		}
		if (lpClassName && strstr(lpClassName, "ShandaADBallon") || lpClassName && strstr(lpClassName, "ShandaADBrowser")) {
			// Found in WinMain
			// AOB 74 ?? 8D ?? ?? 6A 04 ?? 88
			// CMS88(00A13188)
			// Remove Exit Ads
			return NULL;
		}
		return _CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	}

	// Hook: ImmAssociateContext，窗口输入法关联时调用，用于支持中文输入和应用伤害字皮肤
	static auto _ImmAssociateContext = decltype(&ImmAssociateContext)(GetProcAddress(GetModuleHandleW(L"IMM32"), "ImmAssociateContext"));
	HIMC WINAPI ImmAssociateContext_Hook(HWND hWnd, HIMC hIMC) {
		// 首次调用时初始化伤害字皮肤（如有配置），并强制开启 IME 输入法
		// Call by CWndMan::CWndMan<-CWvsApp::CreateWndManager after CWvsApp::InitializeGameData in CWvsApp::SetUp 
		if (!bImmAssociateContextLoaded) {
			// TODO
			bImmAssociateContextLoaded = true;
			if (Config::DamageSkinID > 0) {
				if (!DamageSkin::Init()) {
					DEBUG(L"Unable to init Damage Skin");
				}
				else {
					DamageSkin::ApplyLocally(Config::DamageSkinID);
				}
			}
		}
		// Enable IME input
		HIMC overrideHIMC = ImmGetContext(hWnd);
		ImmSetOpenStatus(overrideHIMC, TRUE);
		return _ImmAssociateContext(hWnd, overrideHIMC);
	}

}

// Hook 命名空间，暴露安装和卸载 Hook 的接口
namespace Hook {

	// 安装所有核心 Hook，拦截关键 API
	void Install() {
		bool ok = SHOOK(true, &_GetStartupInfoA, GetStartupInfoA_Hook) &&
			SHOOK(true, &_CreateMutexA, CreateMutexA_Hook) &&
			SHOOK(true, &_CreateWindowExA, CreateWindowExA_Hook) &&
			SHOOK(true, &_ImmAssociateContext, ImmAssociateContext_Hook);
		if (!ok) {
			DEBUG(L"Failed to install hook");
		}
	}

	// 卸载所有 Hook，恢复原始 API
	void Uninstall() {
		bool ok = SHOOK(false, &_GetStartupInfoA, GetStartupInfoA_Hook) &&
			SHOOK(false, &_CreateMutexA, CreateMutexA_Hook) &&
			SHOOK(false, &_CreateWindowExA, CreateWindowExA_Hook) &&
			SHOOK(false, &_ImmAssociateContext, ImmAssociateContext_Hook);
		if (!ok) {
			DEBUG(L"Failed to uninstall hook");
		}
		Network::ClearupWSA();
	}
}
