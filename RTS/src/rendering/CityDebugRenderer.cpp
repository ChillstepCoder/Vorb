#include "stdafx.h"
#include "CityDebugRenderer.h"

#include "Camera2D.h"
#include "DebugRenderer.h"

#include "box2d/b2_collision.h"

#include "city/CityBuilder.h"
#include "city/CityPlanner.h"

const int PERIOD_FRAMES = 300;

constexpr int MAX_ROOM_COLORS = 8;
constexpr float ROOM_COLOR_ALPHA = 0.4f;
const color4 ROOM_COLORS[MAX_ROOM_COLORS] = {
    color4(1.0f, 0.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, ROOM_COLOR_ALPHA),
};

void renderBlueprint(BuildingBlueprint& bp) {
    // Render the AABB of the floor plan
    b2AABB aabb;
    aabb.lowerBound.x = bp.bottomLeftWorldPos.x;
    aabb.lowerBound.y = bp.bottomLeftWorldPos.y;
    
    aabb.upperBound.x = aabb.lowerBound.x + bp.dims.x;
    aabb.upperBound.y = aabb.lowerBound.y + bp.dims.y;
    DebugRenderer::drawAABB(aabb, color4(0.7f, 0.4f, 0.0f), PERIOD_FRAMES);
    // Render the room graph in world space
    int i = 0;
    for (auto&& node : bp.nodes) {
        b2AABB nodeAabb;
        nodeAabb.lowerBound.x = bp.bottomLeftWorldPos.x + node.offsetFromZero.x;
        nodeAabb.lowerBound.y = bp.bottomLeftWorldPos.y + node.offsetFromZero.y;
        nodeAabb.upperBound.x = nodeAabb.lowerBound.x + 1;
        nodeAabb.upperBound.y = nodeAabb.lowerBound.y + 1;

        const color4& color = ROOM_COLORS[i % MAX_ROOM_COLORS];
        DebugRenderer::drawAABB(nodeAabb, color4(color.r, color.g, color.b, 255u), PERIOD_FRAMES);
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = bp.nodes[node.parentRoom];
            const f32v2 startPos(nodeAabb.lowerBound.x + 0.5f, nodeAabb.lowerBound.y + 0.5f);
            const f32v2 endPos(bp.bottomLeftWorldPos.x + parent.offsetFromZero.x + 0.5f, bp.bottomLeftWorldPos.y + parent.offsetFromZero.y + 0.5f);
            if (node.isPrivate) {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(1.0f, 1.0f, 0.0f), PERIOD_FRAMES);
            }
            else {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES);
            }
        }
        ++i;
    }

    // Render all the metadata
    for (int y = 0; y < bp.dims.y; ++y) {
        for (int x = 0; x < bp.dims.x; ++x) {
            const int index = y * bp.dims.x + x;
            RoomNodeID id = bp.ownerArray[index];
            if (id != INVALID_ROOM_ID) {
                const ui32v2 worldPos = bp.bottomLeftWorldPos + ui32v2(x, y);
                const color4& color = ROOM_COLORS[(int)id % MAX_ROOM_COLORS];
                DebugRenderer::drawQuad(f32v2(worldPos), f32v2(1.0f), color, PERIOD_FRAMES);
            }
        }
    }
}

void CityDebugRenderer::renderCityPlannerDebug(const CityPlanner& cityPlanner) const {
    /*if (frameCount <= 0) {
        for (auto&& bp : cityPlanner.mBluePrints) {
            renderBlueprint(*bp);
        }
    }*/
}

void CityDebugRenderer::renderCityBuilderDebug(const CityBuilder& cityBuilder) const {
    for (auto&& bp : cityBuilder.mInProgressBlueprints) {
        renderBlueprint(*bp);
    }
}
