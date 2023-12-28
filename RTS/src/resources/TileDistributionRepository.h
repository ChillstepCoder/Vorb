#pragma once

#include "definitions/TileDistributionDef.h"
#include "resources/DataAssetRepository.h"

using TileDistributionRepository =
    DataAssetRepository<TileDistributionDef, AssetType::TileDistribution, "tdist">;