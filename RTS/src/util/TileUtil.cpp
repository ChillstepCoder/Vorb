#include "stdafx.h"

#include "TileUtil.h"
#include "world/TileRepository.h"

#include "world/TileCollider.h"
#include "DebugRenderer.h"

#include "options/DebugOptions.h"

IntersectionHit2D TileUtil::tryRayTileIntersect(const Tile& tile, const ui32v2& tilePos, const f32v2& start, const f32v2& end, f32 zOffsetFromTerrain, f32 rayThickness /*= 0.0f*/) {

    const TileCollider* collider = tile.tryGetColliderMainThread();
    if (!collider) return IntersectionHit2D();

    // TODO: Multiple floors?
    f32 baseZPosition = tile.getBaseZPositionUncompressedMainThread(TILE_FLOOR_GROUND);
    
    // Ground check
    float zOffset = zOffsetFromTerrain - baseZPosition;
    if (zOffset < 0.0f) {
        const f32v2 aabbCenter = f32v2(tilePos) + f32v2(0.5f);
        IntersectionHit2D hit = IntersectionUtil::segmentAABBIntersect(start, end - start, aabbCenter, collider->getDimsXy(), f32v2(rayThickness));
        hit.tilePos = tilePos;
        return hit;
    }

    // Top layer tile check
    if (zOffsetFromTerrain <= collider->dims.z) {
        switch (collider->shape) {
            case TileCollisionShape::BOX: {
                const f32v2 aabbCenter = f32v2(tilePos) + f32v2(0.5f);
                IntersectionHit2D hit = IntersectionUtil::segmentAABBIntersect(start, end - start, aabbCenter, collider->getDimsXy(), f32v2(rayThickness));
                hit.tilePos = tilePos;
                return hit;
            }
            case TileCollisionShape::CIRCLE: {
                const f32v2 circleCenter = f32v2(tilePos) + f32v2(0.5f);
                const f32 radius = collider->dims.x;
                if (sDebugOptions.mShowPaths) {
                    DebugRenderer::drawWireQuad(circleCenter - (radius + rayThickness), f32v2((radius + rayThickness) * 2), color4(0.0f, 0.0f, 1.0f, 1.0f), 250);
                }
                IntersectionHit2D hit = IntersectionUtil::segmentCircleIntersect(start, end, circleCenter, radius, rayThickness);
                hit.tilePos = tilePos;
                return hit;
            }
        }
    }
    static_assert((int)TileCollisionShape::COUNT == 3, "Update");
    return IntersectionHit2D();
}
