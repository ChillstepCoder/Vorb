#include "stdafx.h"
#include "ContactListener.h"

#include <box2d/b2_contact.h>

#include "ecs/EntityComponentSystem.h"

// TODO: Shared util?
inline entt::entity extractEntity(b2Fixture* fixture) {
    return (entt::entity)reinterpret_cast<entt::id_type>(fixture->GetUserData());
}

ContactListener::ContactListener(EntityComponentSystem& ecs)
	: mEcs(ecs) {

}

void ContactListener::BeginContact(b2Contact* contact) {
	entt::entity idA = extractEntity(contact->GetFixtureA());
	entt::entity idB = extractEntity(contact->GetFixtureB());

    if (auto* navA = mEcs.mRegistry.try_get<NavigationComponent>(idA)) {
        navA->mColliding = true;
    }
    if (auto* navB = mEcs.mRegistry.try_get<NavigationComponent>(idB)) {
        navB->mColliding = true;
    }
}

void ContactListener::EndContact(b2Contact* contact) {
    entt::entity idA = extractEntity(contact->GetFixtureA());
    entt::entity idB = extractEntity(contact->GetFixtureB());

	if (auto* navA = mEcs.mRegistry.try_get<NavigationComponent>(idA)) {
		navA->mColliding = false;
	}
    if (auto* navB = mEcs.mRegistry.try_get<NavigationComponent>(idB)) {
        navB->mColliding = false;
    }
}
