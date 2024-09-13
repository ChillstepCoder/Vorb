#include "stdafx.h"
#include "TileHarvestable.h"

SERIALIZABLE_ENUM_SAME_NAME(TileHarvestable,
    pair{ TileHarvestable::Wood, "wood"sv },
    pair{ TileHarvestable::WoodBirch, "wood_birch"sv },
    pair{ TileHarvestable::Stone, "stone"sv },
    pair{ TileHarvestable::None, "none"sv }
);
static_assert(e_count(TileHarvestable) == 3);