#include "stdafx.h"
#include "WorldGeneration.h"

#include "world/WorldData.h"

WorldGeneration sWorldGen;

f32 WorldGeneration::getHeightAtPos(const f32v2& worldPos)
{
    // Base height
    f64 height = mBaseNoise.compute((f64)worldPos.x, (f64)worldPos.y);

    f32v2 offsetToCenter(
        worldPos.x - WorldData::WORLD_CENTER.x,
        worldPos.y - WorldData::WORLD_CENTER.y
    );

    //  TODO: Precompute and interpolate, can cubic interpolate and others
    f64 distanceFromCenter2 = glm::length2(offsetToCenter);

    // Preturb the outline via noise
    distanceFromCenter2 += CONTINENT_OUTLINE_SCALE * mContinentOutlineNoise.compute(offsetToCenter.x, offsetToCenter.y);

    // Outline check
    if (distanceFromCenter2 > CONTINENT_RADIUS_SQ) {
        // Ocean
        height -= (distanceFromCenter2 - CONTINENT_RADIUS_SQ) * 0.0000001;
    }
    else {
        // Continent internals
        f64 lerp = (CONTINENT_RADIUS_SQ - distanceFromCenter2) * 0.00000001;
        // Mountains
        f64 mountainDist = mMountainsDistNoise.compute((f64)worldPos.x, (f64)worldPos.y);
        if (mountainDist > 0.0) {
            f64 mountain = mMountainsNoise.compute((f64)worldPos.x, (f64)worldPos.y);
            height += lerp * mountain * glm::min(mountainDist, 1.0);
        }
    }
    return (f32)height;
}
