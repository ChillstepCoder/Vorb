#pragma once

class TileDistributionDef;

class TileDistributionSampler
{
public:
    static void buildPrecalcData(TileDistributionDef& dist);
    static bool sample(const TileDistributionDef& dist, i32v2 worldPos, f32 density, f32 probabilityMult);
    static bool samplePrecalc(const TileDistributionDef& dist, i32v2 worldPos, f32 density);
    static f32 getThresholdAtPosition(const TileDistributionDef& dist, i32v2 worldPos, f32 probabilityMult);
    static f32 getThresholdAtPositionPrecalc(const TileDistributionDef& dist, i32v2 worldPos);
};

