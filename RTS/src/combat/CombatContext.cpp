#include "stdafx.h"
#include "CombatContext.h"

#include "physics/PhysicsWorld.h"

#include "world/IWorld.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/CharacterControlComponent.h"

// For testing
#include "debugging/DebugRenderer.h"

bool anglesInClockwiseSequence(double x, double y, double z) {
    double diff = fmod(y - x, M_2_PI) + fmod(z - y, M_2_PI);
    return diff < M_2_PI;
}

double angularDiffSigned(double theta1, double theta2) {
    double diff = theta2 - theta1;
    while (diff >= M_2_PIF)
        diff -= M_2_PIF;
    while (diff <= 0)
        diff += M_2_PIF;
    return diff;
}

f32AABB3 getAABBEnclosingArc(f32v3 arcOrigin, f32 radius, f32 arcAngleRad, f32 arcRotationRad, f32 arcHeight) {
    f32AABB3 rv;

    // Note that +y is forward so 0 degrees is

    // Always treat arc as centered around the rotation
    arcRotationRad -= arcAngleRad * 0.5f;

    // Normalize the arc rotation
    arcRotationRad = std::fmod(arcRotationRad, M_2_PIF);
    if (arcRotationRad < 0) {
        arcRotationRad += M_2_PIF;
    }

    // Compute start and end points of the arc
    const f32v2 origin2D(arcOrigin);
    f32v2 start = origin2D + glm::vec2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
    f32v2 end = origin2D + glm::vec2(radius * std::cos(arcRotationRad + arcAngleRad), radius * std::sin(arcRotationRad + arcAngleRad));

    DebugRenderer::drawWireQuadThreadSafe(arcOrigin, f32v2(0.5f), color::Red, 200);
    DebugRenderer::drawWireQuadThreadSafe(f32v3(start.x, start.y, arcOrigin.z), f32v2(0.5f), color::Red, 200);
    DebugRenderer::drawWireQuadThreadSafe(f32v3(end.x, end.y, arcOrigin.z), f32v2(0.5f), color::Red, 200);

    // Initialize min and max points of AABB
    f32v2 min = glm::min(glm::min(start, end), origin2D);
    f32v2 max = glm::max(glm::max(start, end), origin2D);

    //// Update the AABB according to the cardinal directions covered by the arc
    //if ((arcRotationRad <= M_PI_2F && arcRotationRad + arcAngleRad >= M_PI_2F)) {
    //    max.y = arcOrigin.y + radius;
    //}
    //if ((arcRotationRad <= M_PIF && arcRotationRad + arcAngleRad >= M_PIF)) {
    //    max.x = arcOrigin.x + radius;
    //}
    //if ((arcRotationRad <= M_PI_2F * 3.0f && arcRotationRad + arcAngleRad >= M_PI_2F * 3.0f)) {
    //    min.y = arcOrigin.y - radius;
    //}
    //if ((arcRotationRad <= M_2_PIF && arcRotationRad + arcAngleRad >= M_2_PIF) || (arcRotationRad <= 0 && arcRotationRad + arcAngleRad >= 0)) {
    //    min.x = arcOrigin.x - radius;
    //}

    //  Check +X edge
    //if ()

    rv.pos.x = min.x;
    rv.pos.y = min.y;
    rv.pos.z = arcOrigin.z;
    rv.dims.x = max.x - min.x;
    rv.dims.y = max.y - min.y;
    rv.dims.z = arcHeight;

    DebugRenderer::drawWireQuadThreadSafe(f32v3(min.x, min.y, arcOrigin.z), f32v2(0.5f), color::LightBlue, 200);
    DebugRenderer::drawWireQuadThreadSafe(f32v3(max.x, max.y, arcOrigin.z), f32v2(0.5f), color::Green, 200);

    return rv;
}

CombatContext::CombatContext(IWorld& world) : mWorld(world) {

}

void CombatContext::performMeleeAttack(entt::entity source, AttackShape shape, f32 radius, f32 arcAngleRad, f32 forwardOffset, BitFlags<AttackFlags> flags) {
    switch (shape) {
        case AttackShape::CONE:
            // TODO: HEIGHT CONFIG
            performConeAttack(source, shape, radius, arcAngleRad, 1.5f, forwardOffset, flags);
            break;
        case AttackShape::SPHERE:
            assert(false);
            break;
        default:
            assert(false);
            break;

    }
    static_assert(e_count(AttackShape) == 2);
}

void CombatContext::performConeAttack(entt::entity source, AttackShape shape, f32 radius, f32 arcAngleRad, f32 height, f32 forwardOffset, BitFlags<AttackFlags> flags) {

    IEntityComponentSystem& ecs = mWorld.getECS();
    const f32 sourceRotation = ecs.mRegistry.get<CharacterControlComponent>(source).mControllerAngle;

    constexpr int MAX_RESULTS = 8;
    PhysicsQueryResult results[MAX_RESULTS];

    // TODO: WorldQueryContext?
    f32AABB3 aabb = getAABBEnclosingArc(ecs.mRegistry.get<PositionComponent>(source).mPosition, radius, arcAngleRad, sourceRotation, height);
    const int resultCount = mWorld.getPhysicsWorld().queryObjectsInAABB(aabb.pos, aabb.pos + aabb.dims, results, MAX_RESULTS);

    // DELETE ALL TILES!!!
    for (int i = 0; i < resultCount; ++i) {
        if (std::holds_alternative<LiteTileHandle>(results[i].mObject)) {
            LiteTileHandle liteHandle = std::get<LiteTileHandle>(results[i].mObject);
            TileHandle tileHandle = liteHandle.toTileHandle(mWorld);
            tileHandle.getMutableContainer()->setTileLayer(tileHandle.tileIndex, TileLayer::Main, TILE_ID_NONE);
        }
    }
}
