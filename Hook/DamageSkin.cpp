// DamageSkin.cpp 负责 MapleStory 客户端的伤害字皮肤功能，包括皮肤资源的初始化和本地应用。
// 通过操作资源管理器接口，实现自定义伤害字皮肤的加载和替换。
//
// 主要功能：
// 1. Init：初始化伤害字皮肤资源，获取可用皮肤 ID 集合。
// 2. ApplyLocally：将指定 ID 的伤害字皮肤应用到本地。
#include <vector>
#include "DamageSkin.h"
#include "ResMan.h"

#include "Share/Tool.h"

namespace {
	// gBasicEffImg：基础特效图片节点
	static IWzPropertyPtr gBasicEffImg;
	// gDamageSkinImg：伤害字皮肤图片节点
	static IWzPropertyPtr gDamageSkinImg;
	// gDamageSkinIDSet：可用伤害字皮肤 ID 集合
	static std::set<std::wstring> gDamageSkinIDSet;
	// kDamageSkinOriPath：原始伤害字图片路径集合
	const std::vector<std::wstring> kDamageSkinOriPath = { L"NoRed0",L"NoRed1",L"NoCri0",L"NoCri1" };
}

namespace DamageSkin {

	// 初始化伤害字皮肤资源，获取可用皮肤 ID 集合
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

	// 将指定 ID 的伤害字皮肤应用到本地
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