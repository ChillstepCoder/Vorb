#include "stdafx.h"
#include "CityDebugRenderer.h"

#include "Camera2D.h"
#include "DebugRenderer.h"

#include "box2d/b2_collision.h"

#include "city/CityBuilder.h"
#include "city/CityPlanner.h"

void renderBlueprint(BuildingBlueprint& bp) {
    // Render the AABB of the floor plan
    b2AABB aabb;
    aabb.lowerBound.x = bp.rootWorldPos.x;
    aabb.lowerBound.y = bp.rootWorldPos.y - bp.dims.y * 0.5f;
    if (bp.dims.y % 2 == 1) {
        aabb.lowerBound.y += 0.5f;
    }
    aabb.upperBound.x = aabb.lowerBound.x + bp.dims.x;
    aabb.upperBound.y = aabb.lowerBound.y + bp.dims.y;
    DebugRenderer::drawAABB(aabb, color4(0.7f, 0.4f, 0.0f), 0);
    // Render the room graph in world space
    for (auto&& node : bp.nodes) {
        b2AABB nodeAabb;
        nodeAabb.lowerBound.x = bp.rootWorldPos.x + node.offsetFromRoot.x;
        nodeAabb.lowerBound.y = bp.rootWorldPos.y + node.offsetFromRoot.y;
        nodeAabb.upperBound.x = nodeAabb.lowerBound.x + 1;
        nodeAabb.upperBound.y = nodeAabb.lowerBound.y + 1;
        if (node.isPrivate) {
            DebugRenderer::drawAABB(nodeAabb, color4(1.0f, 0.0f, 1.0f), 0);
        }
        else {
            DebugRenderer::drawAABB(nodeAabb, color4(0.0f, 0.5f, 1.0f), 0);
        }
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = bp.nodes[node.parentRoom];
            const f32v2 startPos(nodeAabb.lowerBound.x + 0.5f, nodeAabb.lowerBound.y + 0.5f);
            const f32v2 endPos(bp.rootWorldPos.x + parent.offsetFromRoot.x + 0.5f, bp.rootWorldPos.y + parent.offsetFromRoot.y + 0.5f);
            if (node.isPrivate) {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(1.0f, 1.0f, 0.0f));
            }
            else {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(0.0f, 1.0f, 0.0f));
            }
        }
    }
}

void CityDebugRenderer::renderCityPlannerDebug(const CityPlanner& cityPlanner) const {
    for (auto&& bp : cityPlanner.mBluePrints) {
        renderBlueprint(*bp);
    }
}

void CityDebugRenderer::renderCityBuilderDebug(const CityBuilder& cityBuilder) const {
    for (auto&& bp : cityBuilder.mInProgressBlueprints) {
        renderBlueprint(*bp);
    }
}
