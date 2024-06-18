#pragma once

#include "tile/TileDamageData.h"

struct StaticModelInstance {
    f32m4 matrix;
    TileIndex tileIndex;
    ui8 variantIndex;
    TileDamageDataPtr damageData;
};
