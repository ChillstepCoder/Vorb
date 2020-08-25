#pragma once

enum class ItemMaterial : ui8 {
	WOOD,
	LEATHER,
	IRON,
	STEEL,
	SILVER,
	COUNT
};

const float MATERIAL_QUALITY_MULT[enum_cast(ItemMaterial::COUNT)] = {
	1.0f,
	1.0f,
	1.5f,
	2.0f,
	2.2f
};

const float ITEM_MATERIAL_WEIGHT_MULT[enum_cast(ItemMaterial::COUNT)] = {
	0.7f,
	0.7f,
	1.0f,
	1.0f,
	1.0f
};