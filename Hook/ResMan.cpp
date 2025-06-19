#include "ResMan.h"
#include "HookEx.h"

#include "Resources/AOBList.h"
#include "Share/Funcs.h"
#include "Share/Tool.h"

#include<intrin.h>
#pragma intrinsic(_ReturnAddress)

namespace {

	// CMS88 void *__thiscall IWzResMan::GetObjectA(struct IUnknown *this, void *Destination, int *a3, _DWORD *a4, _DWORD *a5)
	// GMS95 Ztl_variant_t *__thiscall IWzResMan::GetObjectA(IWzResMan *this,Ztl_variant_t *result, Ztl_bstr_t sUOL, const Ztl_variant_t *vParam, const Ztl_variant_t *vAux)
	_variant_t* (__thiscall* _IWzResMan__GetObjectA)(void* ecx, _variant_t* result, std::wstring* sUOL, _variant_t* vParam, _variant_t* vAux) = nullptr;

	// CMS88 void *__thiscall IWzProperty::GetItem(struct IUnknown *this, void *Destination, int *a3)
	// GMS95 Ztl_variant_t *__thiscall IWzProperty::GetItem(IWzProperty *this, Ztl_variant_t *result, Ztl_bstr_t sPath)
	_variant_t* (__thiscall* _IWzProperty__GetItem)(void* ecx, _variant_t* result, std::wstring* sPath) = nullptr;

}

namespace ResMan {

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

	_variant_t GetObjectA(std::wstring wsPath) {
		_variant_t vResult;
		_variant_t vParam;
		_variant_t bAux;
		_IWzResMan__GetObjectA(HookEx::GetResMan(), &vResult, &wsPath, &vParam, &bAux);
		return vResult;
	}

	IUnknown* GetUnknown(_variant_t* tagVar) {
		if (tagVar->vt == VT_EMPTY) {
			return nullptr;
		}
		return reinterpret_cast<IUnknown*>(HookEx::GetUnknown(tagVar, false, false));
	}

	IWzPropertyPtr GetWzImage(std::wstring wsPath) {
		_variant_t tagVar = GetObjectA(wsPath);
		IUnknown* pUnknown = GetUnknown(&tagVar);
		if (pUnknown == nullptr) {
			return nullptr;
		}
		return IWzPropertyPtr(pUnknown);
	}

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