#include "stdafx.h"
#include "TileGrassRepository.h"
#include "resources/MaterialRepository.h"

#include <Vorb/io/IOManager.h>

KEG_TYPE_DEF_SAME_NAME(TileGrassFileData, kt) {
    kt.addValue("alpha_masks", keg::Value::basic(offsetof(TileGrassFileData, alphaMasks), keg::BasicType::STRING));
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
        assert(fileData.alphaMasks.size());
        tileGrassData.mAlphaMaterial = materialRepository.getMaterialData(fileData.alphaMasks);
        
        mTileGrassIdMapping[key] = nextId;
        // TODO: Serialize the string > ID mapping
        mTileGrassData.emplace_back(std::move(tileGrassData));
    }));
    return true;
}
