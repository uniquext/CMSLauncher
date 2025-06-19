#include "Tool.h"

std::wstring Str2WStr(const std::string& str) {
	return std::wstring(str.begin(), str.end());
}

std::wstring BYTE2String(BYTE b) {
	std::wstring wb;
	WCHAR high = (b >> 4) & 0x0F;
	WCHAR low = b & 0x0F;

	high += (high <= 0x09) ? 0x30 : 0x37;
	low += (low <= 0x09) ? 0x30 : 0x37;

	wb.push_back(high);
	wb.push_back(low);

	return wb;
}

std::wstring DWORD2String(DWORD dw) {
	std::wstring wdw;

	wdw += BYTE2String((dw >> 24) & 0xFF);
	wdw += BYTE2String((dw >> 16) & 0xFF);
	wdw += BYTE2String((dw >> 8) & 0xFF);
	wdw += BYTE2String(dw & 0xFF);

	return wdw;
}

ULONG_PTR GetFuncAddress(ULONG_PTR uCall) {
	auto uCallByte = (BYTE*)uCall;
	if (uCallByte[0x00] == 0xE8) {
		return uCall + 0x05 + *(ULONG_PTR*)(uCall + 0x01);
	}
	if (uCallByte[0x00] == 0xFF && uCallByte[0x01] == 0x15) {
		return (*(ULONG_PTR*)(uCall + 0x02));
	}
	return 0;
}

ULONG_PTR GetDWORDAddress(ULONG_PTR uPush) {
	return *(DWORD*)(uPush + 1);
}