#pragma once
#include "stdafx.h"
#include "ItemMaterial.h"

typedef ui16 WeaponItemID;

enum class WeaponTypes : ui8 {
	FIST,
	SWORD,
	SPEAR,
	COUNT
};

const float WEAPON_REACHES[enum_cast(WeaponTypes::COUNT)] = {
	0.1f,
	0.2f,
	0.3f
};

const float WEAPON_WEIGHTS[enum_cast(WeaponTypes::COUNT)] = {
	0.0f,
	0.2f,
	0.3f
};

const float WEAPON_BASE_DAMAGES[enum_cast(WeaponTypes::COUNT)] = {
	5.0f,
	25.0f,
	20.0f
};

const float WEAPON_ARMOR_PIERCE[enum_cast(WeaponTypes::COUNT)] = {
	0.0f,
	3.0f,
	12.0f
};

struct WeaponItem {
	WeaponTypes mType = WeaponTypes::FIST;
	ItemMaterial mMaterial = ItemMaterial::LEATHER;
	WeaponItemID mId = 0u;
};

enum class BuiltinWeapons : WeaponItemID {
	IRON_SWORD
};

class WeaponRegistry {
public:
	static const WeaponItem& getWeapon(BuiltinWeapons id) {
		return s_allWeaponItems[enum_cast(id)];
	}

	static const WeaponItem& getWeapon(WeaponItemID id) {
		return s_allWeaponItems[id];
	}

	static void loadWeapon(WeaponTypes type, ItemMaterial material) {
		s_allWeaponItems.emplace_back(WeaponItem{ type, material, static_cast<WeaponItemID>(s_allWeaponItems.size()) });
	}

	static void loadWeapons() {
		// IRON_SWORD
		loadWeapon(WeaponTypes::SWORD, ItemMaterial::IRON);
	}

	static std::vector<WeaponItem> s_allWeaponItems;
};