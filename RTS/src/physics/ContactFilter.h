#pragma once
#include <box2d/b2_world_callbacks.h>

class EntityComponentSystem;

class ContactFilter : public b2ContactFilter
{
public:
	ContactFilter(EntityComponentSystem& ecs);

	bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) override;

private:
    EntityComponentSystem& mEcs;

};

