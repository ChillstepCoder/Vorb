#pragma once

#include "tile/TileDamageData.h"

struct StaticModelInstance {
    f32m4 matrix; // Scale should be pre-applied
    TileIndex tileIndex;
    ui8 variantIndex;
    TileDamageDataPtr damageData;
    f32 scale;
};
