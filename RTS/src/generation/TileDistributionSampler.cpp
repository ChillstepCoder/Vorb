#include "stdafx.h"
#include "TileDistributionSampler.h"

#include "definitions/TileDistributionDef.h"

#include "util/DitherMatrix.h"
#include "math/Random.h"

void TileDistributionSampler::buildPrecalcData(TileDistributionDef& dist) {
    dist.precalculatedDistribution.resizeAndZero(SQ(PRECALCILATED_TILE_DISTRIBUTION_WIDTH));

    for (i32 y = 0, yoff = 0; y < PRECALCILATED_TILE_DISTRIBUTION_WIDTH; ++y, yoff += PRECALCILATED_TILE_DISTRIBUTION_WIDTH) {
        for (i32 x = 0; x < PRECALCILATED_TILE_DISTRIBUTION_WIDTH; ++x) {
            const i32v2 worldPos(x, y);
            i32v2 cellPos = (i32v2(x, y) / dist.spacing) * dist.spacing;

            // Jitter
            if (dist.spacing > 2) {
                const i32v2 oldCellPos = cellPos;
                cellPos.x += (i32)Random::getThreadSafe(oldCellPos.x, oldCellPos.y) % (dist.spacing - 1);
                cellPos.y += (i32)Random::getThreadSafe(oldCellPos.y, 16236 - oldCellPos.x) % (dist.spacing - 1);
            }

            // If we are not sampling valid dither matrix spot, return 0
            if (worldPos != cellPos || Random::getThreadSafef(cellPos.y * 2, cellPos.x - 34253) > dist.probability) {
                dist.precalculatedDistribution.setBitTo(yoff + x, true);
            }
            else {
                dist.precalculatedDistribution.setBitTo(yoff + x, false);
            }
        }
    }
}

bool TileDistributionSampler::sample(const TileDistributionDef& dist, i32v2 worldPos, f32 density) {
    const f32 threshold = getThresholdAtPosition(dist, worldPos);
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


f32 TileDistributionSampler::getThresholdAtPosition(const TileDistributionDef& dist, i32v2 worldPos) {
    assert(dist.spacing >= 0);

    // Offset for less patterned results
   // worldPos.x += worldPos.y * 3.34152f;

    // Round to nearest cell
    i32v2 cellPos = (worldPos / dist.spacing) * dist.spacing;

    // Jitter
    if (dist.spacing > 2) {
        const i32v2 oldCellPos = cellPos;
        cellPos.x += (i32)Random::getThreadSafe(oldCellPos.x, oldCellPos.y) % (dist.spacing - 1);
        cellPos.y += (i32)Random::getThreadSafe(oldCellPos.y, 16236 - oldCellPos.x) % (dist.spacing - 1);
    }

    // If we are not sampling valid dither matrix spot, return 0
    if (worldPos != cellPos) {
        return FLT_MAX;
    }

    if (Random::getThreadSafef(cellPos.y * 2, cellPos.x - 34253) > dist.probability) {
        return FLT_MAX;
    }

    const i32v2 bayerMatrixPos = cellPos % 16;
    const ui8 bayer = BAYER_MATRIX_16[bayerMatrixPos.y * 16 + bayerMatrixPos.x];
    return bayer / 255.f;
}

f32 TileDistributionSampler::getThresholdAtPositionPrecalc(const TileDistributionDef& dist, i32v2 worldPos) {
    assert(dist.spacing >= 0);

    // Offset for less patterned results
   // worldPos.x += worldPos.y * 3.34152f;

    // Round to nearest cell
    worldPos %= PRECALCILATED_TILE_DISTRIBUTION_WIDTH;
    
    // If we are not sampling valid dither matrix spot, return 0
    if (dist.precalculatedDistribution.getBit(worldPos.y * PRECALCILATED_TILE_DISTRIBUTION_WIDTH + worldPos.x)) {
        return FLT_MAX;
    }

    i32v2 cellPos = (worldPos / dist.spacing) * dist.spacing;
    const i32v2 bayerMatrixPos = cellPos % 16;
    const ui8 bayer = BAYER_MATRIX_16[bayerMatrixPos.y * 16 + bayerMatrixPos.x];
    return bayer / 255.f;
}
