#pragma once

#include "tile/TileGrass.h"
#include "util/StrToken.h"

#include "rendering/material/MaterialData.h"

DECL_VIO(class IOManager);

class MaterialRepository;

struct TileGrassFileData {
    nString alphaMasks;
    nString textures;
    f32v2 sizeMults;
    f32 leanVariance;
    i32 density;
    i32 numTextures;
};
KEG_TYPE_DECL(TileGrassFileData);

struct TileGrassData {
    StrToken mName;
    f32v2 mSizeMults;
    f32 mLeanVariance;
    ui8 mDensity; // 1, 2, 4, 8, 16
    ui8 mNumTextures;
    MaterialID mMaterialID;
    TileGrassID mId;
    bool mUseGradientColor = false;
};

class TileGrassRepository
{
    friend class TileEditorPanel;
public:
    const TileGrassData& getTileGrassData(TileGrassID tileId) const {
        assert(tileId < mTileGrassData.size());
        return mTileGrassData[tileId];
    }
    const TileGrassData& getTileGrassData(StrToken tileToken) const {
        // TOOD: Hashed string and error handling
        auto&& it = mTileGrassIdMapping.find(tileToken);
        assert(it != mTileGrassIdMapping.end());
        return mTileGrassData[it->second];
    }
    TileID getTile(StrToken tileToken) const {
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

