#include "stdafx.h"
#include "TileGrassRepository.h"
#include "resources/MaterialRepository.h"

#include <Vorb/io/IOManager.h>

KEG_ENUM_DEF(TileGrassMeshType, TileGrassMeshType, kt) {
    kt.addValue("default", TileGrassMeshType::DEFAULT);
    kt.addValue("plane", TileGrassMeshType::PLANE);
    kt.addValue("billboard", TileGrassMeshType::BILLBOARD);
}
static_assert(e_cast(TileGrassMeshType::COUNT) == 3, "Update keg definition");

KEG_TYPE_DEF_SAME_NAME(TileGrassFileData, kt) {
    kt.addValue("alpha_masks", keg::Value::basic(offsetof(TileGrassFileData, alphaMasks), keg::BasicType::STRING));
    kt.addValue("textures", keg::Value::basic(offsetof(TileGrassFileData, textures), keg::BasicType::STRING));
    kt.addValue("mesh_type", keg::Value::custom(offsetof(TileGrassFileData, meshType), "TileGrassMeshType", true));
    kt.addValue("num_textures", keg::Value::basic(offsetof(TileGrassFileData, numTextures), keg::BasicType::I32));
    kt.addValue("size", keg::Value::basic(offsetof(TileGrassFileData, sizeMults), keg::BasicType::F32_V2));
    kt.addValue("height_variance", keg::Value::basic(offsetof(TileGrassFileData, heightVariance), keg::BasicType::F32_V2));
    kt.addValue("z_offset_variance", keg::Value::basic(offsetof(TileGrassFileData, zOffsetVariance), keg::BasicType::F32_V2));
    kt.addValue("lean_variance", keg::Value::basic(offsetof(TileGrassFileData, leanVariance), keg::BasicType::F32));
    kt.addValue("density", keg::Value::basic(offsetof(TileGrassFileData, density), keg::BasicType::I32));
}

bool TileGrassRepository::loadGrassFile(vio::IOManager& ioManager, const vio::Path& path, const MaterialRepository& materialRepository) {
    // Read file
    return ioManager.parseFileAsKegObjectMap(path, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        TileGrassData tileGrassData;
        TileGrassFileData fileData;

        // Load data
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(TileGrassFileData));
        assert(key.size() < MAX_CHARS_IN_STRTOKEN, "Grass name does not fit in StrToken");
        StrToken token(key);

        // Copy data
        tileGrassData.mName = token;

        TileGrassID nextId = (TileGrassID)mTileGrassData.size();
        tileGrassData.mId = nextId;
        assert(nextId < INVALID_TILE_GRASS_ID); // Make sure we dont roll over
        assert(mTileGrassIdMapping.find(token) == mTileGrassIdMapping.end()); // Duplicate name
        assert(fileData.alphaMasks.size() || fileData.textures.size());
        if (fileData.alphaMasks.size()) {
            tileGrassData.mMaterialID = materialRepository.getMaterialDesc(fileData.alphaMasks).id;
            tileGrassData.mUseGradientColor = true;
        }
        else {
            tileGrassData.mMaterialID = materialRepository.getMaterialDesc(fileData.textures).id;
            tileGrassData.mUseGradientColor = false;
        }
        tileGrassData.mNumTextures = fileData.numTextures;
        tileGrassData.mMeshType = fileData.meshType;
        tileGrassData.mSizeMults = fileData.sizeMults;
        tileGrassData.mHeightVariance = fileData.heightVariance;
        tileGrassData.mZOffsetVariance = fileData.zOffsetVariance;
        tileGrassData.mLeanVariance = fileData.leanVariance;
        tileGrassData.mDensity = fileData.density;
        assert(tileGrassData.mDensity <= MAX_GRASS_DETAIL);
        
        mTileGrassIdMapping[key] = nextId;
        // TODO: Serialize the string > ID mapping
        mTileGrassData.emplace_back(std::move(tileGrassData));
    }));
    return true;
}
