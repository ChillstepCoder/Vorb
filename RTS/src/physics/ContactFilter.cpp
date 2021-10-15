#include "stdafx.h"
#include "ContactFilter.h"

bool ContactFilter::ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) {
    bool shouldCollide = b2ContactFilter::ShouldCollide(fixtureA, fixtureB);
    if (!shouldCollide) {
        return false;
    }
    // 3D checking
}
