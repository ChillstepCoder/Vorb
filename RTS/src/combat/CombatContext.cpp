#include "stdafx.h"
#include "CombatContext.h"

#include "physics/PhysicsWorld.h"

#include "world/IWorld.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/CharacterControlComponent.h"

#include "btBulletCollisionCommon.h"
#include "BulletCollision/CollisionShapes/btCylinderShape.h"
// For testing
#include "debugging/DebugRenderer.h"

f32AABB3 getAABBEnclosingArc(f32v3 arcOrigin, f32 radius, f32 arcAngleRad, f32 arcRotationRad, f32 arcHeight) {
    f32AABB3 rv;
    
    // We check center point, the two ray extrema, and then potentially the 4 extreme points on each axis
    // depending on the covered angle

    // Always treat arc as centered around the rotation
    arcRotationRad -= arcAngleRad * 0.5f;

    // Normalize the arc rotation
    arcRotationRad = std::fmod(arcRotationRad, M_2_PIF);
    if (arcRotationRad < 0) {
        arcRotationRad += M_2_PIF;
    }

    const f32 arcRotationEndRad = arcRotationRad + arcAngleRad;

    // Compute start and end points of the arc
    const f32v2 origin2D(arcOrigin);
    f32v2 start = origin2D + glm::vec2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
    f32v2 end = origin2D + glm::vec2(radius * std::cos(arcRotationEndRad), radius * std::sin(arcRotationEndRad));

    DebugRenderer::drawWireQuadThreadSafe(arcOrigin, f32v2(0.5f), color::Red, 200);
    DebugRenderer::drawWireQuadThreadSafe(f32v3(start.x, start.y, arcOrigin.z), f32v2(0.5f), color::Red, 200);
    DebugRenderer::drawWireQuadThreadSafe(f32v3(end.x, end.y, arcOrigin.z), f32v2(0.5f), color::Red, 200);

    // Initialize min and max points of AABB
    f32v2 min = glm::min(glm::min(start, end), origin2D);
    f32v2 max = glm::max(glm::max(start, end), origin2D);

    // Two base cases, either we overlap the X, or not.
    // When we overlap X, arcRotationEnd will be greater than 2PI
    // +x
    if (arcRotationEndRad > M_2_PIF) {
        max.x = origin2D.x + radius;
        // +Y case
        if (arcRotationEndRad > M_2_PIF + M_PI_2F || arcRotationRad < M_PI_2F) {
            max.y = origin2D.y + radius;
        }
        // -X case
        if (arcRotationEndRad > M_2_PIF + M_PI || arcRotationRad < M_PI) {
            min.x = origin2D.x - radius;
        }
        // -Y case
        if (arcRotationEndRad > M_2_PIF + M_3_PI_2F || arcRotationRad < M_3_PI_2F) {
            min.y = origin2D.y - radius;
        }
    }
    else {
        // +Y case
        if (arcRotationRad < M_PI_2F && arcRotationEndRad > M_PI_2F) {
            max.y = origin2D.y + radius;
        }
        // -X case
        if (arcRotationRad < M_PIF && arcRotationEndRad > M_PIF) {
            min.x = origin2D.x - radius;
        }
        // -Y case
        if (arcRotationRad < M_3_PI_2F && arcRotationEndRad > M_3_PI_2F) {
            min.y = origin2D.y - radius;
        }
    }

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
    // TODO: Offset
    const f32v3 attackStartPos = ecs.mRegistry.get<PositionComponent>(source).mPosition;
    const f32v2 forwardNormal = f32v2(cos(sourceRotation), sin(sourceRotation));
    const f32AABB3 aabb = getAABBEnclosingArc(attackStartPos, radius, arcAngleRad, sourceRotation, height);
    const int resultCount = mWorld.getPhysicsWorld().queryObjectsInAABB(aabb.pos, aabb.pos + aabb.dims, results, MAX_RESULTS);

    
    // DELETE ALL TILES!!!
    for (int i = 0; i < resultCount; ++i) {
        const btCollisionShape* shape = results[i].mCollisionObject->getCollisionShape();
        if (shape == nullptr) {
            return;
        }

        const f32v2 centerPoint2D = btVector3ToF32v3(results[i].mCollisionObject->getWorldTransform().getOrigin());
        const f32v2 offsetToTarget = centerPoint2D - f32v2(attackStartPos);
        const f32 distanceFromTarget2 = glm::length2(offsetToTarget);

        // Tile handle
        if (std::holds_alternative<LiteTileHandle>(results[i].mObject)) {

            bool intersectsArc = false;
            const int shapeType = shape->getShapeType();
            switch (shapeType) {
                case CYLINDER_SHAPE_PROXYTYPE: {
                    const btCylinderShape* cylinder = static_cast<const btCylinderShape*>(shape);
                    const btVector3 halfExtents = cylinder->getHalfExtentsWithoutMargin();
                    assert(halfExtents.x() == halfExtents.y());

                    const f32 totalRadius = radius + halfExtents.x();
                    if (distanceFromTarget2 <= SQ(totalRadius)) {
                        const f32v2 normalToTarget = offsetToTarget / sqrt(distanceFromTarget2);
                        // TODO: add rotation padding based on the radius?
                        if (acosf(glm::dot(normalToTarget, forwardNormal)) < arcAngleRad * 0.5f) {
                            intersectsArc = true;
                        }
                    }
                    break;
                    // ... (add other cases as needed)
                }
                default:
                    assert(false && "Unhandled shape type");
            }

            if (intersectsArc) {
                LiteTileHandle liteHandle = std::get<LiteTileHandle>(results[i].mObject);
                TileHandle tileHandle = liteHandle.toTileHandle(mWorld);
                tileHandle.getMutableContainer()->setTileLayer(tileHandle.tileIndex, TileLayer::Main, TILE_ID_NONE);
            }
        }
        else {
            // TODO: ENTITY
        }
    }
}
