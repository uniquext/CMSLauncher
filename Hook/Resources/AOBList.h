#pragma once
#include <string>
#include <array>

#pragma region Network
const std::array<std::wstring, 2> AOB_Scan_CPatchException__CPatchException_Addrs = {
	// CMS79(005291C4)->CMS85(00563123)
	L"66 C7 07 ?? 00 66 89 46 06 FF",
	// CMS86(0051FB39)->CMS88(0052B0D9)->CMS96(0054FB99)->CMS100(00571A09)->CMS104(0058BC58)
	L"BA ?? 00 00 00 66 89 17 66 89 46 06",
};
#pragma endregion

#pragma region ResMan
const std::array<std::wstring, 2> AOB_Scan_IWzResMan__GetObjectA_Addrs = {
	// CMS79(00404A75)->CMS85(00403AE9)
	L"B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 EC 14 83 ?? ?? 00 53 56 57 8B D9",
	// CMS86(00404AD0)->CMS88(00404A00)->CMS104(00404E70)
	L"6A FF 68 ?? ?? ?? 00 64 A1 00 00 00 00 50 83 EC 14 53 55 56 A1 ?? ?? ?? ?? 33 C4",
};
const std::array<std::wstring, 2> AOB_Scan_IWzProperty__GetItem_Addrs = {
	// CMS79(00403AC7)->CMS85(00403881)
	L"B8 ?? ?? ?? 00 E8 ?? ?? ?? 00 83 EC 14 83 65 F0 00 56 57 8B F1",
	// CMS86(00404960)->CMS88(00404890)->CMS104(00404D00)
	L"6A FF 68 ?? ?? ?? 00 64 A1 00 00 00 00 50 83 EC 14 56 A1",
};
#pragma endregion

#pragma region Wnd
#pragma endregion

#pragma region Code
const std::wstring AOB_Code_Ret = L"31 C0 C3"; // xor eax,eax ret
#pragma endregion

