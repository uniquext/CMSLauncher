#include <vector>
#include "DamageSkin.h"
#include "ResMan.h"

#include "Share/Tool.h"

namespace {
	static IWzPropertyPtr gBasicEffImg;
	static IWzPropertyPtr gDamageSkinImg;
	static std::set<std::wstring> gDamageSkinIDSet;
	const std::vector<std::wstring> kDamageSkinOriPath = { L"NoRed0",L"NoRed1",L"NoCri0",L"NoCri1" };
}

namespace DamageSkin {

	bool Init() {
		gBasicEffImg = ResMan::GetWzImage(L"Effect/BasicEff.img");
		if (gBasicEffImg == nullptr) {
			DEBUG(L"Failed to get BasicEff.img");
			return false;
		}

		gDamageSkinImg = ResMan::GetWzImage(L"Effect/DamageSkin.img");
		if (gDamageSkinImg == nullptr) {
			DEBUG(L"Failed to get DamageSkin.img");
			return false;
		}

		gDamageSkinIDSet = ResMan::GetWzImageEnum(gDamageSkinImg);

		return true;
	}

	void ApplyLocally(const unsigned int id) {
		std::wstring newID = std::to_wstring(id);
		if (!gDamageSkinIDSet.count(newID)) {
			DEBUG(L"Unknown damage skin id " + newID);
			return;
		}

		for (std::wstring wsOriPath : kDamageSkinOriPath) {
			std::wstring wsNewPath = newID + L"/" + wsOriPath;
			_variant_t tagVar;
			CHECK_HRESULT(gDamageSkinImg->get_item(wsNewPath.c_str(), &tagVar));
			CHECK_HRESULT(gBasicEffImg->put_item(wsOriPath.c_str(), tagVar));
		}
	}
}