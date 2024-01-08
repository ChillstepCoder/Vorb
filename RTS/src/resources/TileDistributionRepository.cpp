#include "stdafx.h"
#include "TileDistributionRepository.h"

#include "generation/TileDistributionSampler.h"

void TileDistributionRepository::fixupAsset(AssetID id) {
    TileDistributionSampler::buildPrecalcData(*mAssets[id]);
}
