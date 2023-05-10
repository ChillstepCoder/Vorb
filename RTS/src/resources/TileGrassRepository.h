#pragma once

#include "tile/TileGrass.h"
#include "util/StrToken.h"

#include "rendering/material/MaterialData.h"

#include "generation/NoiseFunction.hpp"

DECL_VIO(class IOManager);

class MaterialRepository;

struct TileGrassFileData {
    nString alphaMasks;
    nString textures;
    f32v2 sizeMults = f32v2(1.0f);
    f32v2 heightVariance = f32v2(0.2f, 0.8f);
    f32 leanVariance = 0.5f;
    i32 density = 8;
    i32 numTextures = 1;
};
KEG_TYPE_DECL(TileGrassFileData);

struct TileGrassData {
    StrToken mName;
    f32v2 mSizeMults;
    f32v2 mHeightVariance;
    f32 mLeanVariance;
    ui8 mDensity; //  MAX_GRASS_DETAIL
    ui8 mNumTextures;
    MaterialID mMaterialID;
    TileGrassID mId;
    bool mUseGradientColor = false;
    NoiseFunction mNoiseFunction = NoiseFunction("Grass", 6, 0.7, 0.05, { 1200.0, -1200.0 }, 1.0, 0.0);
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

