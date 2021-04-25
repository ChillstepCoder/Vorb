#include "stdafx.h"

#include "SoldierAIComponent.h"
#include "CombatComponent.h"

#include "World.h"
#include "ecs/EntityComponentSystem.h"

// TODO: Debug render
const float AGGRO_DISTANCE = 5000.0f;
const float MIN_DISTANCE = 0.2f; // TODO: Matches NavigationComponent
const float DISTANCE_THRESHOLD = 0.2f; // TODO: Matches NavigationComponent=

inline void updateComponent(entt::entity entity, SoldierAIComponent& cmp, World& world, EntityComponentSystem& ecs) {
	//// Check if dead
	//PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	//// TODO: No allocations
	//std::vector<EntityDistSortKey> nearbyEntities = world.queryActorsInRadius(myPhysCmp.getPosition(), AGGRO_DISTANCE, ActorTypes::ACTORTYPE_UNDEAD, ActorTypes::ACTORTYPE_CORPSE, true, entity);
	//if (nearbyEntities.size()) {
	//	vecs::EntityID closest = nearbyEntities.front().second;
	//	const float distance = nearbyEntities.front().first.dist - myPhysCmp.mCollisionRadius;
	//	if (distance > MIN_DISTANCE + DISTANCE_THRESHOLD) {
	//		const PhysicsComponent& targetPhysCmp = ecs.getPhysicsComponentFromEntity(closest);
	//		f32v2 offset = myPhysCmp.getPosition() - targetPhysCmp.getPosition();
	//		offset = glm::normalize(offset);
	//		// Try to get next to it
	//		offset *= myPhysCmp.mCollisionRadius + targetPhysCmp.mCollisionRadius + MIN_DISTANCE;

	//		NavigationComponent& myNavCmp = ecs.getNavigationComponentFromEntity(entity);
	//		myNavCmp.mHasTarget = true;
	//		myNavCmp.mTargetPos = targetPhysCmp.getPosition() + offset;
	//	}
	//	else if (cmp.mAttackCooldown <= 0.0f) {
	//		PhysicsComponent& targetPhysCmp = ecs.getPhysicsComponentFromEntity(closest);
	//		f32v2 dir = glm::normalize(myPhysCmp.getPosition() - targetPhysCmp.getPosition());
	//		float flankingAngle = glm::dot(dir, targetPhysCmp.mDir);

	//		SimpleSpriteComponent& targetSpriteComp = ecs.getSimpleSpriteComponentFromEntity(closest);
	//		if (Combat::resolveMeleeAttack(ecs.getCombatComponentFromEntity(entity), ecs.getCombatComponentFromEntity(closest), targetPhysCmp, targetSpriteComp, dir, RAD_TO_DEG(flankingAngle))) {
	//			ecs.convertEntityToCorpse(closest);
	//		}
	//		// TODO: REAL
	//		cmp.mAttackCooldown = 3.0f;
	//	}
	//	else {
	//		cmp.mAttackCooldown -= 0.1f;
	//	}
	//}
}

void SoldierAIComponentTable::update(EntityComponentSystem& ecs, World& world) {
	//// Update components
	//for (auto&& cmp : *this) {
	//	if (isValid(cmp)) {
	//		updateComponent(cmp.first, cmp.second, world, ecs);
	//	}
	//}
}
