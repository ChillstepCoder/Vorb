#pragma once
#include "ItemMaterial.h"

typedef ui16 ArmorItemID;

enum class ArmorTypes : ui8 {
	NONE,
	LIGHT,
	MEDIUM,
	HEAVY,
	COUNT
};

const float ARMOR_BASE_REDUCTION[enum_cast(ArmorTypes::COUNT)] = {
	0.0f,
	5.0f,
	10.0f,
	20.0f
};

struct ArmorItem {
	ArmorTypes mType = ArmorTypes::NONE;
	ItemMaterial mMaterial = ItemMaterial::WOOD;
	ArmorItemID mId = 0u;
	//float mThicknessMult = 1.0f;
};

enum class BuiltinArmors : ArmorItemID {
	IRON_ARMOR_MEDIUM
};
class ArmorRegistry {
public:
	static const ArmorItem& getArmor(BuiltinArmors id) {
		return s_allArmorItems[enum_cast(id)];
	}

	static const ArmorItem& getArmor(ArmorItemID id) {
		return s_allArmorItems[id];
	}

	static void loadArmor(ArmorTypes type, ItemMaterial material) {
		s_allArmorItems.emplace_back(ArmorItem{ type, material, static_cast<ArmorItemID>(s_allArmorItems.size()) });
	}

	static void loadArmors() {
		// IRON_SWORD
		loadArmor(ArmorTypes::MEDIUM, ItemMaterial::IRON);
	}

	static std::vector<ArmorItem> s_allArmorItems;
};