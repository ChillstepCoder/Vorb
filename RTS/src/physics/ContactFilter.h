#pragma once
#include <box2d/b2_world_callbacks.h>

class ContactFilter : public b2ContactFilter
{
public:
	bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) override;

};

