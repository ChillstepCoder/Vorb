#pragma once

// TODO: Could this become TileInteractable? ResourceTag?
enum class TileHarvestable : ui8 {
    Wood,
    WoodBirch,
    Stone,
    None,
    COUNT = None
};
SERIALIZABLE_ENUM_SAME_NAME(TileHarvestable,
    pair{ TileHarvestable::Wood, "wood"sv },
    pair{ TileHarvestable::Wood, "wood_birch"sv },
    pair{ TileHarvestable::Stone, "stone"sv },
    pair{ TileHarvestable::None, "none"sv }
);
static_assert(e_count(TileHarvestable) == 3);