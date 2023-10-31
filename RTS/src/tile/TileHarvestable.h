#pragma once

// TODO: Could this become TileInteractable? ResourceTag?
enum class TileHarvestable : ui8 {
    WOOD,
    STONE,
    NONE,
    COUNT = NONE
};
SERIALIZABLE_ENUM_SAME_NAME(TileHarvestable,
    pair{ TileHarvestable::WOOD, "wood"sv },
    pair{ TileHarvestable::STONE, "stone"sv },
    pair{ TileHarvestable::NONE, "none"sv }
);
static_assert(e_count(TileHarvestable) == 2);