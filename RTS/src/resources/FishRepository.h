#pragma once

#include "definitions/FishDef.h"

DECL_VIO(class IOManager);

class TextureRepository;
class ItemRepository;

class FishRepository
{
public:
    FishRepository(vio::IOManager& ioManager);
    ~FishRepository();

    void loadFishFile(const vio::Path& filePath, ItemRepository& itemRepo, TextureRepository& textureRepo);

    const FishDef& getFish(FishID id) const { assert(fishExists(id)); return mFishDefinitions[id]; }
    const FishDef& getFish(const nString& itemName) const;

    bool fishExists(FishID id) const { return id < mFishDefinitions.size(); }


private:
    vio::IOManager& mIoManager;

    std::map<nString, FishID> mFishIdLookup;
    std::vector<FishDef> mFishDefinitions;
};

