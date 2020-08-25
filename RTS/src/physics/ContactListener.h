#pragma once
#include <box2d/b2_world_callbacks.h>

class EntityComponentSystem;

class ContactListener : public b2ContactListener {
public:
	ContactListener(EntityComponentSystem& ecs);

	void BeginContact(b2Contact* contact) override;

	void EndContact(b2Contact* contact) override;

	void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override {

	}

	void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override {

	}

private:
	EntityComponentSystem& mEcs;
};