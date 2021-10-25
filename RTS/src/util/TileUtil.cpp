#include "stdafx.h"

#include "TileUtil.h"
#include "world/TileRepository.h"

#include "DebugRenderer.h"

IntersectionHit2D TileUtil::tryRayTileIntersect(const TileCollision& collision, const ui32v2& tilePos, const f32v2& start, const f32v2& end, f32 zPos, f32 rayThickness /*= 0.0f*/) {

    // Ground check
    float zOffset = zPos - collision.baseZPosition;
    if (zOffset < 0.0f) {
        const f32v2 aabbCenter = f32v2(tilePos) + f32v2(0.5f);
        IntersectionHit2D hit = IntersectionUtil::segmentAABBIntersect(start, end - start, aabbCenter, collision.getColliderDimsScaledXY(), f32v2(rayThickness));
        hit.tilePos = tilePos;
        return hit;
    }

    // Top layer tile check
    if (zPos <= collision.getColliderHeightScaled()) {
        switch (collision.shape) {
            case TileCollisionShape::BOX: {
                const f32v2 aabbCenter = f32v2(tilePos) + f32v2(0.5f);
                IntersectionHit2D hit = IntersectionUtil::segmentAABBIntersect(start, end - start, aabbCenter, collision.getColliderDimsScaledXY(), f32v2(rayThickness));
                hit.tilePos = tilePos;
                return hit;
            }
            case TileCollisionShape::CIRCLE: {
                const f32v2 circleCenter = f32v2(tilePos) + f32v2(0.5f);
                const f32 radius = collision.getColliderDimsScaledXY().x;
                DebugRenderer::drawWireQuad(circleCenter - (radius + rayThickness), f32v2((radius + rayThickness) * 2), color4(0.0f, 0.0f, 1.0f, 1.0f), 250);
                IntersectionHit2D hit = IntersectionUtil::segmentCircleIntersect(start, end, circleCenter, radius, rayThickness);
                hit.tilePos = tilePos;
                return hit;
            }
        }
    }
    static_assert((int)TileCollisionShape::COUNT == 3, "Update");
    return IntersectionHit2D();
}
