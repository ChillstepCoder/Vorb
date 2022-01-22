#include "stdafx.h"
#include "ContactListener.h"

#include <box2d/b2_contact.h>

#include "ecs/EntityComponentSystem.h"

// TODO: Shared util?
inline entt::entity extractEntity(b2Fixture* fixture) {
    return (entt::entity)static_cast<entt::id_type>(fixture->GetUserData().pointer);
}

ContactListener::ContactListener(EntityComponentSystem& ecs)
	: mEcs(ecs) {

}

void ContactListener::BeginContact(b2Contact* contact) {
	entt::entity idA = extractEntity(contact->GetFixtureA());
	entt::entity idB = extractEntity(contact->GetFixtureB());

    if (auto* navA = mEcs.mRegistry.try_get<NavigationComponent>(idA)) {
        navA->mFlags |= (~NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT);
    }
    if (auto* navB = mEcs.mRegistry.try_get<NavigationComponent>(idB)) {
        navB->mFlags |= (~NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT);
    }
}

void ContactListener::EndContact(b2Contact* contact) {
    entt::entity idA = extractEntity(contact->GetFixtureA());
    entt::entity idB = extractEntity(contact->GetFixtureB());

	if (auto* navA = mEcs.mRegistry.try_get<NavigationComponent>(idA)) {
		navA->mFlags &= (~NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT);
	}
    if (auto* navB = mEcs.mRegistry.try_get<NavigationComponent>(idB)) {
        navB->mFlags &= (~NAVIGATION_COMPONENT_FLAG_COLLIDING_WITH_AGENT);
    }
}
