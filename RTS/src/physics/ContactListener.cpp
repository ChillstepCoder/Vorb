#include "ContactListener.h"

#include <box2d/b2_contact.h>
#include <Vorb/ecs/ECS.h>

#include "EntityComponentSystem.h"

ContactListener::ContactListener(EntityComponentSystem& ecs)
	: mEcs(ecs) {

}

void ContactListener::BeginContact(b2Contact* contact) {
	b2Fixture* fixtureA = contact->GetFixtureA();
	b2Fixture* fixtureB = contact->GetFixtureB();

	vecs::EntityID idA = reinterpret_cast<vecs::EntityID>(fixtureA->GetUserData());
	vecs::EntityID idB = reinterpret_cast<vecs::EntityID>(fixtureB->GetUserData());

	NavigationComponent& navA = mEcs.getNavigationComponentFromEntity(idA);
	NavigationComponent& navB = mEcs.getNavigationComponentFromEntity(idB);

	if (&navA != &mEcs.mNavigationTable.getDefaultData()) {
		navA.mColliding = true;
	}
	if (&navB != &mEcs.mNavigationTable.getDefaultData()) {
		navB.mColliding = true;
	}
}

void ContactListener::EndContact(b2Contact* contact) {
	b2Fixture* fixtureA = contact->GetFixtureA();
	b2Fixture* fixtureB = contact->GetFixtureB();

	vecs::EntityID idA = reinterpret_cast<vecs::EntityID>(fixtureA->GetUserData());
	vecs::EntityID idB = reinterpret_cast<vecs::EntityID>(fixtureB->GetUserData());

	NavigationComponent& navA = mEcs.getNavigationComponentFromEntity(idA);
	NavigationComponent& navB = mEcs.getNavigationComponentFromEntity(idB);

	if (&navA != &mEcs.mNavigationTable.getDefaultData()) {
		navA.mColliding = false;
	}
	if (&navB != &mEcs.mNavigationTable.getDefaultData()) {
		navB.mColliding = false;
	}
}
