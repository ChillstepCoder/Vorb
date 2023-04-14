#pragma once

#include "tile/TileGrass.h"
#include "util/StrToken.h"

#include "rendering/material/MaterialData.h"

DECL_VIO(class IOManager);

class MaterialRepository;

struct TileGrassFileData {
    nString alphaMasks;
};
KEG_TYPE_DECL(TileGrassFileData);

struct TileGrassData {
    StrToken mName;
    MaterialData mAlphaMaterial;
    TileGrassID mId;
};

class TileGrassRepository
{
public:
    const TileGrassData& getTileGrassData(TileGrassID tileId) {
        assert(tileId < mTileGrassData.size());
        return mTileGrassData[tileId];
    }
    const TileGrassData& getTileGrassData(StrToken tileToken) {
        // TOOD: Hashed string and error handling
        TileID id = mTileGrassIdMapping[tileToken];
        return mTileGrassData[id];
    }
    TileID getTile(StrToken tileToken) {
        auto&& it = mTileGrassIdMapping.find(tileToken);
        assert(it != mTileGrassIdMapping.end());
        return it->second;
    }

    const std::vector<TileGrassData>& getAllGrassData() const { return mTileGrassData; }

    bool loadGrassFile(vio::IOManager& ioManager, const vio::Path& path, const MaterialRepository& materialRepository);

private:
    std::unordered_map<StrToken, TileID> mTileGrassIdMapping;
    std::vector<TileGrassData> mTileGrassData;
};

