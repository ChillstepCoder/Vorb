#include "stdafx.h"
#include "TileDistributionSampler.h"

#include "definitions/TileDistributionDef.h"

#include "util/DitherMatrix.h"
#include "math/Random.h"

bool TileDistributionSampler::sample(TileDistributionDef& dist, i32v2 worldPos, f32 density) {
    const f32 threshold = getThresholdAtPosition(dist, worldPos);
    return density >= threshold;
}

f32 TileDistributionSampler::getThresholdAtPosition(TileDistributionDef& dist, i32v2 worldPos) {

    // Jitter WRONG
    //worldPos.x += (i32)Random::getThreadSafe(worldPos.x, worldPos.y) % 3 - 1;
    //worldPos.y += (i32)Random::getThreadSafe(worldPos.y, 78236 - worldPos.y) % 3 - 1;

    const i32v2 bayerMatrixPos = worldPos % 16;
    const ui8 bayer = BAYER_MATRIX_16[bayerMatrixPos.y * 16 + bayerMatrixPos.x];
    return bayer / 255.f;
}
