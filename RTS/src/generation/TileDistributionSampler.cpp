#include "stdafx.h"
#include "TileDistributionSampler.h"

#include "definitions/TileDistributionDef.h"

#include "util/DitherMatrix.h"
#include "math/Random.h"

constexpr i32 PRECALC_TILE_DIST_WIDTH = 256;
constexpr i32 MAX_FORCE_ITERATIONS = 1;

void TileDistributionSampler::buildPrecalcData(TileDistributionDef& dist) {
    PreciseTimer timer;
    dist.precalculatedDistribution.resize(SQ(PRECALC_TILE_DIST_WIDTH));

    for (i32 y = 0, yoff = 0; y < PRECALC_TILE_DIST_WIDTH; ++y, yoff += PRECALC_TILE_DIST_WIDTH) {
        for (i32 x = 0; x < PRECALC_TILE_DIST_WIDTH; ++x) {
            const i32v2 pos(x, y);
            i32v2 bayerPos = (pos / dist.spacing);
            i32v2 cellPos = bayerPos * dist.spacing;

            // Jitter
            if (dist.spacing > 1 + dist.minDistance) {
                const i32v2 oldCellPos = cellPos;
                const i32 maxWiggle = (dist.spacing - dist.minDistance);
                cellPos.x += (i32)Random::getThreadSafe(oldCellPos.x, oldCellPos.y) % maxWiggle;
                cellPos.y += (i32)Random::getThreadSafe(oldCellPos.y, 16236 - oldCellPos.x) % maxWiggle;
                cellPos.x += bayerPos.y % maxWiggle;
            }
            else {
                cellPos.x += bayerPos.y % dist.spacing;
            }

            dist.precalculatedDistribution.setBitTo(yoff + x, pos == cellPos);
        }
    }

    LOG_DEBUG("TileDistributionSampler::buildPrecalcData finished  in {} ms", timer.stop());
}

bool TileDistributionSampler::sample(const TileDistributionDef& dist, i32v2 worldPos, f32 density, f32 probabilityMult) {
    const f32 threshold = getThresholdAtPosition(dist, worldPos, probabilityMult);
    if (threshold >= FLT_MAX) {
       return false;
    };
    if (dist.distFunc) {
        density *= dist.distFunc->compute(worldPos.x, worldPos.y);
    }
    return density >= threshold;
}

bool TileDistributionSampler::samplePrecalc(const TileDistributionDef& dist, i32v2 worldPos, f32 density) {
    const f32 threshold = getThresholdAtPositionPrecalc(dist, worldPos);
    if (threshold >= FLT_MAX) {
        return false;
    };
    if (dist.distFunc) {
        density *= dist.distFunc->compute(worldPos.x, worldPos.y);
    }
    return density >= threshold;
}


f32 TileDistributionSampler::getThresholdAtPosition(const TileDistributionDef& dist, i32v2 worldPos, f32 probabilityMult) {
    assert(dist.spacing >= 0);

    // Offset for less patterned results
   // worldPos.x += worldPos.y * 3.34152f;

    // Round to nearest cell
    i32v2 bayerPos = (worldPos / dist.spacing);
    i32v2 cellPos = bayerPos * dist.spacing;

    // Jitter
    if (dist.spacing > 1 + dist.minDistance) {
        const i32v2 oldCellPos = cellPos;
        const i32 maxWiggle = (dist.spacing - dist.minDistance);
        cellPos.x += (i32)Random::getThreadSafe(oldCellPos.x, oldCellPos.y) % maxWiggle;
        cellPos.y += (i32)Random::getThreadSafe(oldCellPos.y, 16236 - oldCellPos.x) % maxWiggle;
        cellPos.x += bayerPos.y % maxWiggle;
    }
    else {
        cellPos.x += bayerPos.y % dist.spacing;
    }

    // If we are not sampling valid dither matrix spot, return 0
    if (worldPos != cellPos) {
        return FLT_MAX;
    }

    if (Random::getThreadSafef(cellPos.y * 2, cellPos.x - 34253) > dist.probability * probabilityMult) {
        return FLT_MAX;
    }

    const i32v2 bayerMatrixPos = bayerPos % 16;
    const ui8 bayer = BAYER_MATRIX_16[bayerMatrixPos.y * 16 + bayerMatrixPos.x];
    return bayer / 255.f;
}

f32 TileDistributionSampler::getThresholdAtPositionPrecalc(const TileDistributionDef& dist, i32v2 worldPos) {
    assert(dist.spacing >= 0);

    // Offset for less patterned results
   // worldPos.x += worldPos.y * 3.34152f;

    // Round to nearest cell
    const i32v2 precalcPos = worldPos % PRECALC_TILE_DIST_WIDTH;
    
    // If we are not sampling valid dither matrix spot, return 0
    if (!dist.precalculatedDistribution.getBit(precalcPos.y * PRECALC_TILE_DIST_WIDTH + precalcPos.x)) {
        return FLT_MAX;
    }

    if (Random::getThreadSafef(worldPos.y * 2, worldPos.x - 34253) > dist.probability) {
        return FLT_MAX;
    }

    const i32v2 bayerMatrixPos = (precalcPos / dist.spacing) % 16;
    const ui8 bayer = BAYER_MATRIX_16[bayerMatrixPos.y * 16 + bayerMatrixPos.x];
    return bayer / 255.f;
}
