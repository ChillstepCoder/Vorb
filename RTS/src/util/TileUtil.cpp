#include "stdafx.h"

#include "TileUtil.h"

#include "DebugRenderer.h"

IntersectionHit TileUtil::tryRayTileIntersect(const Tile& tile, const ui32v2& tilePos, const f32v2& start, const f32v2& end, f32 rayThickness /*= 0.0f*/) {
    
    // Get the biggest collider shape
    TileCollisionShape biggestShape = TileCollisionShape::COUNT;
    f32 biggestRadius = 0.0f;
    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        TileID layerTile = tile.layers[i];
        if (layerTile == TILE_ID_NONE) {
            continue;
        }
        // First opaque tile
        const TileData& tileData = TileRepository::getTileData(layerTile);
        const f32 colliderRadius = TileCollisionShapeRadii[enum_cast(tileData.collisionShape)];
        if (colliderRadius > biggestRadius) {
            biggestRadius = colliderRadius;
            biggestShape = tileData.collisionShape;
        }
    }

    // No collide
    if (biggestShape == TileCollisionShape::COUNT) {
        return IntersectionHit();
    }

    switch (biggestShape) {
        case TileCollisionShape::FLOOR:
        case TileCollisionShape::BOX: {
            const f32v2 aabbCenter = f32v2(tilePos) + f32v2(0.5f);
            const f32v2 dims(biggestRadius);
            return IntersectionUtil::segmentAABBIntersect(start, end - start, aabbCenter, dims, f32v2(rayThickness));
        }
                                    // Circle falls through
        case TileCollisionShape::SMALL_CIRCLE:
        case TileCollisionShape::MEDIUM_CIRCLE: {
            const f32v2 circleCenter = f32v2(tilePos) + f32v2(0.5f);
            DebugRenderer::drawBox(circleCenter - (biggestRadius + rayThickness), f32v2((biggestRadius + rayThickness) * 2), color4(0.0f, 0.0f, 1.0f, 1.0f), 250);
            return IntersectionUtil::segmentCircleIntersect(start, end, circleCenter, biggestRadius, rayThickness);
        }

        default:
            assert(false); // Unhandled shape
            break;
    }
    static_assert((int)TileCollisionShape::COUNT == 4, "Update");
    return IntersectionHit();
}
