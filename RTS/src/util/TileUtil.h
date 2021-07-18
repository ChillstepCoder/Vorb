#pragma once

#include "world/Tile.h"
#include "util/IntersectionUtil.h"

namespace TileUtil {
    IntersectionHit tryRayTileIntersect(const Tile& tile, const ui32v2& tilePos, const f32v2& start, const f32v2& end, f32 rayThickness = 0.0f);
}