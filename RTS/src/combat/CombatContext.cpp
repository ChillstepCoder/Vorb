#include "stdafx.h"
#include "CombatContext.h"

#include "physics/PhysicsWorld.h"

#include "world/IWorld.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/CharacterControlComponent.h"

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

    // Always treat arc as centered around the rotation
    arcRotationRad -= arcAngleRad * 0.5f;

    // Normalize the arc rotation (Fails if more than 2PI off)
    if (arcRotationRad < 0) {
        arcRotationRad += M_2_PIF;
    }
    else if (arcRotationRad > M_2_PIF) {
        arcRotationRad -= M_2_PIF;
    }

    const f32v2 arcOrigin2D(arcOrigin);
    f32v2 E = arcOrigin2D + f32v2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
    f32v2 F = arcOrigin2D + f32v2(radius * std::cos(arcRotationRad + arcAngleRad), radius * std::sin(arcRotationRad + arcAngleRad));

    double x1 = E.x, y1 = E.y, x2 = E.x, y2 = E.y;

    if (F.x < x1)
        x1 = F.x;
    if (F.x > x2)
        x2 = F.x;
    if (F.y < y1)
        y1 = F.y;
    if (F.y > y2)
        y2 = F.y;

    double thetaE = atan2(E.y - arcOrigin.y, E.x - arcOrigin.x);
    double thetaF = atan2(F.y - arcOrigin.y, F.x - arcOrigin.x);

    if (anglesInClockwiseSequence(thetaE, 0, thetaF)) {
        double x = (arcOrigin.x + radius);
        if (x > x2)
            x2 = x;
    }

    if (anglesInClockwiseSequence(thetaE, M_PI_2, thetaF)) {
        double y = (arcOrigin.y + radius);
        if (y > y2)
            y2 = y;
    }

    if (anglesInClockwiseSequence(thetaE, M_PI, thetaF)) {
        double x = (arcOrigin.x - radius);
        if (x < x1)
            x1 = x;
    }

    if (anglesInClockwiseSequence(thetaE, 3 * M_PI_2, thetaF)) {
        double y = (arcOrigin.y - radius);
        if (y < y1)
            y1 = y;
    }

    rv.pos.x = x1;
    rv.pos.y = y1;
    rv.pos.z = arcOrigin.z;
    rv.dims.x = x2 - x1;
    rv.dims.y = y2 - y1;
    rv.dims.z = arcHeight;

    return rv;
}

//f32AABB3 getAABBEnclosingArc(f32v3 arcOrigin, f32 radius, f32 arcAngleRad, f32 arcRotationRad, f32 arcHeight) {
//    f32AABB3 rv;
//
//    // Always treat arc as centered around the rotation
//    arcRotationRad -= arcAngleRad * 0.5f;
//
//    // Normalize the arc rotation (Fails if more than 2PI off)
//    if (arcRotationRad < 0) {
//        arcRotationRad += M_2_PIF;
//    }
//    else if (arcRotationRad > M_2_PIF) {
//        arcRotationRad -= M_2_PIF;
//    }
//
//    // Compute start and end points of the arc
//    const f32v2 origin2D(arcOrigin);
//    f32v2 start = origin2D + glm::vec2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
//    f32v2 end = origin2D + glm::vec2(radius * std::cos(arcRotationRad + arcAngleRad), radius * std::sin(arcRotationRad + arcAngleRad));
//
//    // Initialize min and max points of AABB to start and end points of arc
//    f32v2 min = glm::min(start, end);
//    f32v2 max = glm::max(start, end);
//
//    // If arc covers the 90 or 270 degrees points of circle, update AABB accordingly
//    if ((arcRotationRad <= M_PI_2F && arcRotationRad + arcAngleRad >= M_PI_2F) || (arcRotationRad <= M_PI_2F * 3.0f && arcRotationRad + arcAngleRad >= M_PI_2F * 3.0f)) {
//        max.y = arcOrigin.y + radius;
//    }
//    if ((arcRotationRad <= M_PIF && arcRotationRad + arcAngleRad >= M_PIF) || (arcRotationRad <= M_2_PIF && arcRotationRad + arcAngleRad >= M_2_PIF)) {
//        min.x = arcOrigin.x - radius;
//    }
//    if ((arcRotationRad <= 0 && arcRotationRad + arcAngleRad >= 0) || (arcRotationRad <= M_PIF && arcRotationRad + arcAngleRad >= M_PIF)) {
//        max.x = arcOrigin.x + radius;
//    }
//    if ((arcRotationRad <= M_PI_2F * 3.0f && arcRotationRad + arcAngleRad >= M_PI_2F * 3.0f) || (arcRotationRad <= M_PI_2 && arcRotationRad + arcAngleRad >= M_PI_2)) {
//        min.y = arcOrigin.y - radius;
//    }
//
//    rv.pos.x = min.x;
//    rv.pos.y = min.y;
//    rv.pos.z = arcOrigin.z;
//    rv.dims.x = max.x - min.x;
//    rv.dims.y = max.y - min.y;
//    rv.dims.z = arcHeight;
//
//    return rv;
//}

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
