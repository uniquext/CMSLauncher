#pragma once
#include <set> 

#include "com/com.h"
#include "Share/Rosemary.h"

namespace ResMan {
	bool Extend(Rosemary& r);
	_variant_t GetObjectA(std::wstring wsPath);
	IUnknown* GetUnknown(_variant_t* tagVar);
	IWzPropertyPtr GetWzImage(std::wstring wsPath);
	std::set<std::wstring> GetWzImageEnum(IWzPropertyPtr pProperty);
}