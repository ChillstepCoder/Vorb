#include "stdafx.h"
#include "TileDistributionRepository.h"

#include "generation/TileDistributionSampler.h"

void TileDistributionRepository::fixupRegisteredAsset(AssetID id) {
    TileDistributionSampler::buildPrecalcData(*mAssets[id]);
}
