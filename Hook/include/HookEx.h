#pragma once
#include "Share/Rosemary.h"


namespace HookEx {
#pragma region AntiCheat
	bool RemoveLocaleCheck(Rosemary& r);
	bool RemoveSecurityClient(Rosemary& r);
	bool RemoveManipulatePacketCheck(Rosemary& r);
	bool RemoveRenderFrameCheck(Rosemary& r);
	bool RemoveEnterFieldCheck(Rosemary& r);
#pragma endregion

#pragma region ResManEx
	bool Mount(Rosemary& r, const BYTE* mapleVersion, const unsigned int* mapleWZKey);
	void* GetResMan();
	void* GetUnknown(void* tagVar, bool fAddRef, bool fTryChangeType);
#pragma endregion

}