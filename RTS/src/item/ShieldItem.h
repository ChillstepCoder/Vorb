#pragma once
#include "ItemMaterial.h"

typedef ui16 ShieldItemID;

enum class ShieldTypes : ui8 {
	NONE,
	BUCKLER,
	ROUND,
	KITE,
	TOWER,
	COUNT
};

const float SHIELD_PROJECTILE_DEFLECTIONS_CHANCES[static_cast<int>(ShieldTypes::COUNT)] = {
	0.0f,
	0.2f,
	0.4f,
	0.55f,
	0.8f
};

const float SHIELD_DEFLECTIONS_ANGLES[static_cast<int>(ShieldTypes::COUNT)] = {
	-1.0f, // Never deflect
	50.0f,
	90.0f,
	100.0f,
	120.0f,
};

const float SHIELD_WEIGHTS[static_cast<int>(ShieldTypes::COUNT)] = {
	0.0f,
	0.5f,
	0.9f,
	1.3f,
	2.0f,
};

struct ShieldItem {
	ShieldTypes mType = ShieldTypes::NONE;
	ItemMaterial mMaterial = ItemMaterial::WOOD;
	ShieldItemID mId = 0u;
};

enum class BuiltinShields : ShieldItemID {
	IRON_ROUND
};

class ShieldRegistry {
public:
	static const ShieldItem& getShield(BuiltinShields id) {
		return s_allShieldItems[enum_cast(id)];
	}

	static const ShieldItem& getShield(ShieldItemID id) {
		return s_allShieldItems[id];
	}

	static void loadShield(ShieldTypes type, ItemMaterial material) {
		s_allShieldItems.emplace_back(ShieldItem{ type, material, static_cast<ShieldItemID>(s_allShieldItems.size()) });
	}

	static void loadShields() {
		// IRON_SWORD
		loadShield(ShieldTypes::ROUND, ItemMaterial::IRON);
	}

	static std::vector<ShieldItem> s_allShieldItems;
};