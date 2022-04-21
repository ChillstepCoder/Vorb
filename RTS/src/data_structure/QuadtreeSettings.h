#pragma once

#include "world/WorldData.h"


struct QuadtreeSettings {
    f32 distance;
    f32 distanceSq;
    f32 fadeDistance;
    f32 lodDistanceOffset;
};