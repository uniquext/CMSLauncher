// ResMan.cpp 负责 MapleStory 客户端的资源管理相关 Hook，包括资源对象获取、WZ 图片节点遍历等。
// 通过 Hook 资源管理器相关接口，实现对游戏资源的扩展和自定义。
//
// 主要功能：
// 1. Extend：扩展资源管理器，定位并 Hook 关键资源函数。
// 2. GetObjectA：根据路径获取资源对象。
// 3. GetUnknown：从 _variant_t 获取 IUnknown 指针。
// 4. GetWzImage：获取指定路径的 WZ 图片节点。
// 5. GetWzImageEnum：遍历 WZ 图片节点下的所有子项。
#include "ResMan.h"
#include "HookEx.h"

#include "Resources/AOBList.h"
#include "Share/Funcs.h"
#include "Share/Tool.h"

#include<intrin.h>
#pragma intrinsic(_ReturnAddress)

namespace {

	// IWzResMan::GetObjectA 和 IWzProperty::GetItem 的函数指针，用于后续调用
	_variant_t* (__thiscall* _IWzResMan__GetObjectA)(void* ecx, _variant_t* result, std::wstring* sUOL, _variant_t* vParam, _variant_t* vAux) = nullptr;
	_variant_t* (__thiscall* _IWzProperty__GetItem)(void* ecx, _variant_t* result, std::wstring* sPath) = nullptr;

}

namespace ResMan {

	// 扩展资源管理器，定位并 Hook 关键资源函数
	bool Extend(Rosemary& r) {
		// IWzResMan::GetObjectA
		ULONG_PTR uGetObjectAddress;
		for (size_t i = 0; i < AOB_Scan_IWzResMan__GetObjectA_Addrs.size(); i++) {
			uGetObjectAddress = r.Scan(AOB_Scan_IWzResMan__GetObjectA_Addrs[i]);
			if (uGetObjectAddress > 0) {
				break;
			}
		}
		if (uGetObjectAddress == 0) {
			return false;
		}
		// IWzProperty::GetItem
		ULONG_PTR uGetItemAddress;
		for (size_t i = 0; i < AOB_Scan_IWzProperty__GetItem_Addrs.size(); i++) {
			uGetItemAddress = r.Scan(AOB_Scan_IWzProperty__GetItem_Addrs[i]);
			if (uGetItemAddress > 0) {
				break;
			}
		}
		if (uGetItemAddress == 0) {
			return false;
		}
		SADDR(&_IWzResMan__GetObjectA, uGetObjectAddress); // CMS88(00404A00)
		SADDR(&_IWzProperty__GetItem, uGetItemAddress); // CMS88(00404890)
		return true;
	}

	// 根据路径获取资源对象
	_variant_t GetObjectA(std::wstring wsPath) {
		_variant_t vResult;
		_variant_t vParam;
		_variant_t bAux;
		_IWzResMan__GetObjectA(HookEx::GetResMan(), &vResult, &wsPath, &vParam, &bAux);
		return vResult;
	}

	// 从 _variant_t 获取 IUnknown 指针
	IUnknown* GetUnknown(_variant_t* tagVar) {
		if (tagVar->vt == VT_EMPTY) {
			return nullptr;
		}
		return reinterpret_cast<IUnknown*>(HookEx::GetUnknown(tagVar, false, false));
	}

	// 获取指定路径的 WZ 图片节点
	IWzPropertyPtr GetWzImage(std::wstring wsPath) {
		_variant_t tagVar = GetObjectA(wsPath);
		IUnknown* pUnknown = GetUnknown(&tagVar);
		if (pUnknown == nullptr) {
			return nullptr;
		}
		return IWzPropertyPtr(pUnknown);
	}

	// 遍历 WZ 图片节点下的所有子项
	std::set<std::wstring> GetWzImageEnum(IWzPropertyPtr pProperty) {
		std::set<std::wstring> result;
		IUnknown* pUnkEnum;
		CHECK_HRESULT(pProperty->get__NewEnum(&pUnkEnum));
		IEnumVARIANTPtr pEnum = IEnumVARIANTPtr(pUnkEnum);
		_variant_t tagVar;
		ULONG fetched = 0;
		while (pEnum && pEnum->Next(1, &tagVar, &fetched) == S_OK && fetched == 1) {
			if (tagVar.vt == VT_BSTR) {
				result.insert(tagVar.bstrVal);
			}
			else {
				SCANRES(tagVar.vt);
				break;
			}
		}
		return result;
	}

}