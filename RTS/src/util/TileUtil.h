#pragma once

#include "world/TileCollision.h"
#include "util/IntersectionUtil.h"

namespace TileUtil {
    IntersectionHit2D tryRayTileIntersect(const TileCollision& collision, const ui32v2& tilePos, const f32v2& start, const f32v2& end, f32 zPos, f32 rayThickness = 0.0f);
}