// Launcher.cpp 负责 MapleStory 客户端的启动与注入流程。
// 主要功能是检测运行环境、查找目标程序和 DLL，并通过 Injector 实现 DLL 注入。
//
// 主要流程：
// 1. 获取当前可执行文件路径和目录。
// 2. 检查 MapleStory.exe 和 Hook.dll 是否存在。
// 3. 调用 Injector 注入 DLL 并启动游戏。
#include "Injector.h"
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#define IS_DEV false
#define GAME_DIR L"H:\\Maple\\MapleClient\\CMS88"
#define GAME_NAME L"MapleStory.exe"
#define DLL_NAME L"Hook.dll"
#define OFFICIAL_ADDR L" 221.231.130.70 8484" 

// 判断指定路径是否存在
bool PathExists(const std::wstring& path) {
	return PathFileExists(path.c_str()) == TRUE;
}

// 获取当前可执行文件完整路径
std::wstring GetExecutablePath() {
	WCHAR path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	return std::wstring(path);
}

// 获取当前可执行文件所在目录
std::wstring GetExecutableDir() {
	std::wstring executablePath = GetExecutablePath();
	size_t pos = executablePath.find_last_of(L"\\/");
	return (std::wstring::npos == pos) ? L"" : executablePath.substr(0, pos);
}

int main() {
	FreeConsole(); // 隐藏控制台窗口
	std::wstring exeDir;
	std::wstring wDllPath;
	std::wstring wTargetPath;
	exeDir = GetExecutableDir();
	wDllPath = exeDir + L"\\" + DLL_NAME;
#if IS_DEV
	wTargetPath = std::wstring(GAME_DIR) + L"\\" + GAME_NAME;
#else
	wTargetPath = exeDir + L"\\" + GAME_NAME;
	// 检查 MapleStory.exe 是否存在
	if (!PathExists(wTargetPath)) {
		MessageBox(NULL, L"Please run in MapleStory directory", L"Launcher", MB_OK | MB_ICONINFORMATION);
		return 1;
	}
	// 检查 Hook.dll 是否存在
	if (!PathExists(wDllPath)) {
		std::wstring msg = L"Can't find file " + std::wstring(DLL_NAME);
		MessageBox(NULL, msg.c_str(), L"Launcher", MB_OK | MB_ICONERROR);
		return 1;
	}
#endif
	// 创建 Injector 并注入 DLL
	Injector injector(wTargetPath, wDllPath);
	if (!injector.Run(OFFICIAL_ADDR)) {
		MessageBox(NULL, L"Unable to inject dll into target", L"Launcher", MB_OK | MB_ICONERROR);
		return 1;
	};
	return 0;
}