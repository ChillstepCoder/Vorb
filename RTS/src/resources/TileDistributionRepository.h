#pragma once

#include "definitions/TileDistributionDef.h"
#include "resources/DataAssetRepository.h"

class TileDistributionRepository : public DataAssetRepository<TileDistributionDef, AssetType::TileDistribution, "tdist"> {
public:
    using DataAssetRepository<TileDistributionDef, AssetType::TileDistribution, "tdist">::DataAssetRepository;
    static void initInstance(vio::IOManager& ioManager) {
        IAssetRepository<TileDistributionDef>::sInstance = std::make_unique<TileDistributionRepository>(ioManager);
    }
    inline static TileDistributionRepository& get() {
        assert(IAssetRepository<TileDistributionDef>::sInstance);
        return (TileDistributionRepository&)*IAssetRepository<TileDistributionDef>::sInstance;
    }

    void fixupAsset(AssetID id) override;
};