// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>
#include "Hook.h"

static void WINAPI MainProc(LPVOID lpParameter) {
	Hook::Install();
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	HANDLE hThread = NULL;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)&MainProc, nullptr, 0, nullptr);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		// No specific handling
		break;
	case DLL_PROCESS_DETACH:
		if (hThread) {
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
			hThread = NULL;
			Hook::Uninstall();
		}
		break;
	}
	return TRUE;
}

