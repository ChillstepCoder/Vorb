#include "stdafx.h"
#include "UndeadAIComponent.h"

#include "TileGrid.h"
#include "EntityComponentSystem.h"

// TODO: Debug render
const float AGGRO_DISTANCE = 5000.0f;
const float MIN_DISTANCE = 0.2f; // TODO: Matches NavigationComponent
const float DISTANCE_THRESHOLD = 0.3f; // TODO: Matches NavigationComponent=
const std::string& UndeadAIComponentTable::NAME = "undeadai";

inline void updateComponent(vecs::EntityID entity, UndeadAIComponent& cmp, TileGrid& world, EntityComponentSystem& ecs) {
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	// TODO: No allocations
	std::vector<EntityDistSortKey> nearbyEntities = world.queryActorsInRadius(myPhysCmp.getPosition(), AGGRO_DISTANCE, ActorTypes::ACTORTYPE_HUMAN, true, entity);
	if (nearbyEntities.size()) {
		vecs::EntityID closest = nearbyEntities.front().second;
		const float distance = nearbyEntities.front().first - myPhysCmp.mCollisionRadius;
		if (distance > MIN_DISTANCE + DISTANCE_THRESHOLD) {
			const PhysicsComponent& targetPhysCmp = ecs.getPhysicsComponentFromEntity(closest);
			f32v2 offset = myPhysCmp.getPosition() - targetPhysCmp.getPosition();
			offset = glm::normalize(offset);
			// Try to get next to it
			offset *= myPhysCmp.mCollisionRadius + targetPhysCmp.mCollisionRadius + MIN_DISTANCE;

			NavigationComponent& myNavCmp = ecs.getNavigationComponentFromEntity(entity);
			myNavCmp.mHasTarget = true;
			myNavCmp.mTargetPos = targetPhysCmp.getPosition() + offset;
		}
	}
}

void UndeadAIComponentTable::update(EntityComponentSystem& ecs, TileGrid& world) {
	// Update components
	for (auto&& cmp : *this) {
		updateComponent(cmp.first, cmp.second, world, ecs);
	}
}
