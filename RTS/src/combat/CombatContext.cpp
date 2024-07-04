#include "stdafx.h"
#include "CombatContext.h"

#include "world/World.h"
#include "effect/IEffectContext.h"

#include "ecs/IFullECS.h"
#include "ecs/component/CharacterControlComponent.h"


#include "options/DebugOptions.h"

#include "math/Random.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBodyFilters.h"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

// For testing
#include "debugging/DebugRenderer.h"

constexpr int DEBUG_LIFETIME_QUERIES = 100;

// Returns negative number
int getRandomDamageValue(ui16v2 range) {
    assert(range.x >= 0 && range.y >= range.x);
    return -(int)roundf(Random::getCachedRandomf() * ((f32)range.y - (f32)range.x) + (f32)range.x);
}

void debugDrawArc(f32v3 arcOrigin, f32 radius, f32 arcAngleRad, f32 arcRotationRad, f32 arcHeight, color4 color, int lifeTime) {

    if (!sDebugOptions.mShowCombatQueries) {
        return;
    }

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
    const f32v2 start2D = origin2D + glm::vec2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
    const f32v2 end2D = origin2D + glm::vec2(radius * std::cos(arcRotationEndRad), radius * std::sin(arcRotationEndRad));

    const f32v3 start3D(start2D.x, start2D.y, arcOrigin.z);
    const f32v3 end3D(end2D.x, end2D.y, arcOrigin.z);
    const f32v3 arcHeight3D(0.0f, 0.0f, arcHeight);

    // Sides
    AM::DebugRenderer::drawFilledQuadThreadSafe(arcOrigin, start3D, start3D + arcHeight3D, arcOrigin + arcHeight3D, color, lifeTime);
    AM::DebugRenderer::drawFilledQuadThreadSafe(arcOrigin, end3D, end3D + arcHeight3D, arcOrigin + arcHeight3D, color, lifeTime);

    constexpr f32 STEP = DEG_TO_RAD(5.0f);

    // Arc
    f32v3 point = start3D;
    f32 angle = glm::min(STEP, arcAngleRad);
    for (; angle <= arcAngleRad; angle += STEP) {
        const f32v2 next2D = origin2D + glm::vec2(radius * std::cos(arcRotationRad + angle), radius * std::sin(arcRotationRad + angle));
        const f32v3 next3D(next2D.x, next2D.y, arcOrigin.z);
        AM::DebugRenderer::drawFilledQuadThreadSafe(point, next3D, next3D + arcHeight3D, point + arcHeight3D, color, lifeTime);
        point = next3D;
    }
    // Last quad if we didnt evenly end up there
    if (angle != arcAngleRad) {
        AM::DebugRenderer::drawFilledQuadThreadSafe(point, end3D, end3D + arcHeight3D, point + arcHeight3D, color, lifeTime);
    }

}

f32AABB3 getAABBEnclosingArc(f32v3 arcOrigin, f32 radius, f32 arcAngleRad, f32 arcRotationRad, f32 arcHeight) {
    f32AABB3 rv;
    
    debugDrawArc(arcOrigin, radius, arcAngleRad, arcRotationRad, arcHeight, color4(255, 255, 0, 95), DEBUG_LIFETIME_QUERIES);

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
    const f32v2 start = origin2D + glm::vec2(radius * std::cos(arcRotationRad), radius * std::sin(arcRotationRad));
    const f32v2 end = origin2D + glm::vec2(radius * std::cos(arcRotationEndRad), radius * std::sin(arcRotationEndRad));

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

    return rv;
}

CombatContext::CombatContext(World& world) : WorldContextObject(world) {

}

void CombatContext::performAttack(entt::entity source, const SkillDef& skillDef, const AttackData& attackData) {
    switch (attackData.shapeType) {
        case AttackShape::SPHERE:
            assert(false);
            break;
        case AttackShape::CONE:
            performConeAttack(source, skillDef, attackData);
            break;
        default:
            assert(false);
            break;

    }
    static_assert(e_count(AttackShape) == 2);
}

void CombatContext::performConeAttack(entt::entity source, const SkillDef& skillDef, const AttackData& attackData) {
    assert(attackData.shapeType == AttackShape::CONE);

    const AttackShapeCone& coneData = std::get<AttackShapeCone>(attackData.varAttackShape);

    IFullECS& ecs = mWorld.getECS();
    const f32 sourceRotation = ecs.mRegistry.get<CharacterControlComponent>(source).mControllerAngleRad;
    const PhysBodyID sourceBody = ecs.mRegistry.get<PhysicsComponent>(source).mBodyID;

    constexpr int MAX_RESULTS = 8;
    PhysicsQueryResult results[MAX_RESULTS];

    // TODO: WorldQueryContext?
    // TODO: Offset
    const f32v3 attackStartPos = ecs.mRegistry.get<PositionComponent>(source).mPosition;
    const f32v2 attackStartPos2D(attackStartPos);
    const f32v2 forwardNormal = f32v2(cos(sourceRotation), sin(sourceRotation));
    const f32AABB3 aabb = getAABBEnclosingArc(attackStartPos, coneData.radius, coneData.arcAngleRad, sourceRotation, coneData.height);
    const f32 halfArcAngleRad = coneData.arcAngleRad * 0.5f;
    const int resultCount = mWorld.getPhysicsWorld().queryObjectsInAABB(aabb.pos, aabb.pos + aabb.dims, std::span(results), {}, {}, PhysicsBodyFilterAttackable(sourceBody));

    const f32v2 origin2D(attackStartPos);
    const f32v2 start2D = origin2D + glm::vec2(coneData.radius * std::cos(sourceRotation - halfArcAngleRad), coneData.radius * std::sin(sourceRotation - halfArcAngleRad));
    const f32v2 end2D = origin2D + glm::vec2(coneData.radius * std::cos(sourceRotation + halfArcAngleRad), coneData.radius * std::sin(sourceRotation + halfArcAngleRad));
    
    // Damage tile
    for (int i = 0; i < resultCount; ++i) {
       
        // This is at the center of the collision object, not the base
        const f32v3 targetCenter = results[i].mCenterOfMassPosition;
        const f32v2 targetCenterPoint2D = targetCenter;
        const f32v2 offsetToTarget2D = targetCenterPoint2D - f32v2(attackStartPos);
        const f32 distanceFromTarget2 = glm::length2(offsetToTarget2D);
        const f32v2 normalToTarget2D = offsetToTarget2D / sqrt(distanceFromTarget2);

        f32v3 impactPosition = f32v3(0.0f);
        f32v3 impactNormal = f32v3(0.0f);
        f32v3 impactDir = f32v3(0.0f);

        const PhysicsBodyUserData bodyUserData = results[i].mBodyUserData;
        const PhysicsBodyUserDataType type = bodyUserData.getType();

        // Tile handle
        if (type == PhysicsBodyUserDataType::Tile) {
            const JPH::Shape* shape = results[i].mShape;
            PhysicsShapeUserData shapeUserData(shape->GetUserData());

            bool intersectsArc = false;
            const CollisionShapes shapeType = shapeUserData.getShapeType();
            switch (shapeType) {
                //case CYLINDER_SHAPE_PROXYTYPE: {
                case CollisionShapes::CAPSULE: {
                    const JPH::CapsuleShape* capsule = static_cast<const JPH::CapsuleShape*>(shape);
                    const f32 capsuleRadius = capsule->GetRadius();
                    const f32 targetRadiusSQ = SQ(capsuleRadius);

                    const f32 totalRadius = coneData.radius + capsuleRadius;
                    if (distanceFromTarget2 <= targetRadiusSQ) {
                        // If attack origin intersects the cylinder
                        intersectsArc = true;
                    }
                    else if (distanceFromTarget2 <= SQ(totalRadius)) {
                        // If target center is within the front arc
                        if (acosf(glm::dot(normalToTarget2D, forwardNormal)) < coneData.arcAngleRad * 0.5f) {
                            intersectsArc = true;
                        }
                        else {
                            // Check intersect with both segments
                            if (MathUtil::computePointToLineSegmentDistanceSQ(targetCenterPoint2D, start2D, attackStartPos2D) < targetRadiusSQ) {
                                intersectsArc = true;
                            }
                            else if (MathUtil::computePointToLineSegmentDistanceSQ(targetCenterPoint2D, end2D, attackStartPos2D) < targetRadiusSQ) {
                                intersectsArc = true;
                            }
                        }
                    }
                    // Compute hit info
                    if (intersectsArc) {
                        const f32v2 impactNormal2D = offsetToTarget2D / sqrt(distanceFromTarget2);
                        impactNormal = f32v3(impactNormal2D.x, impactNormal2D.y, 0.0f);
                        f32v3 targetCenterAtSourceHeight(targetCenter.x, targetCenter.y, attackStartPos.z);
                        const f32v3 impactCenter = targetCenterAtSourceHeight + f32v3(0.0f, 0.0f, attackData.swingHeight);
                        impactPosition = impactCenter - impactNormal * static_cast<f32>(capsuleRadius);
                    }
                    break;
                    // ... (add other cases as needed)
                }
                default:
                    assert(false && "Unhandled shape type");
            }

            // Hit!
            if (intersectsArc) {
                const f32 angleRad = MathUtil::yawFromDirection(normalToTarget2D);
                impactDir = MathUtil::rotateVectorYawRad(attackData.swingDir, angleRad);
                LiteTileHandle hitTileHandle(bodyUserData.getTileData());
                hitTile(hitTileHandle, skillDef, attackData.damageRange, impactPosition, impactNormal, impactDir);
            }
        }
        else if (type == PhysicsBodyUserDataType::Entity) {
            const entt::entity hitEntity = bodyUserData.getEntity();
            // No self hit
            if (hitEntity == source) {
                continue;
            }
            // TODO: ENTITY
            //assert(false);
            LOG_CRITICAL("TODO: HANDLE ENTITY ATTACKS");
        }
    }
}

void CombatContext::hitTile(LiteTileHandle liteHandle, const SkillDef& skillDef, ui16v2 damageRange, f32v3 impactPosition, f32v3 impactNormal, f32v3 impactDir) {
    TileHandle tileHandle = liteHandle.toTileHandle(mWorld);
    tileHandle.getMutableContainer()->adjustTileHealth(
        tileHandle.tileIndex,
        TileLayer::Main,
        getRandomDamageValue(damageRange),
        impactPosition,
        impactNormal
    );

    if (skillDef.mHitEffect.isValid()) {
        ParticleSystemInputs inputs;
        inputs.mInputImpactDirection = impactDir;
        inputs.mInputImpactSurfaceNormal = impactNormal;
        mWorld.getEffectContext().playParticleEffectAtPoint(skillDef.mHitEffect, impactPosition, inputs, BitFlags<EffectCreateFlags>());
    }
}
