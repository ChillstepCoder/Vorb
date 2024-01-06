#pragma once

class TileDistributionDef;

class TileDistributionSampler
{
public:
    static bool sample(TileDistributionDef& dist, i32v2 worldPos, f32 density);
    static f32 getThresholdAtPosition(TileDistributionDef& dist, i32v2 worldPos);
};

