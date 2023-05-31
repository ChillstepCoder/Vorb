#pragma once

#include "definitions/FishDef.h"

DECL_VIO(class IOManager);

class TextureRepository;
class ItemRepository;
class ModelRepository;

class FishRepository
{
    friend class TileEditorPanel;
public:
    FishRepository(vio::IOManager& ioManager);
    ~FishRepository();

    void loadFishFile(const vio::Path& filePath, ModelRepository& modelRepo, ItemRepository& itemRepo, TextureRepository& textureRepo);

    const FishDef& getFish(FishID id) const { assert(fishExists(id)); return mFishDefinitions[id]; }
    const FishDef& getFish(const nString& itemName) const;
    const std::vector<FishDef>& getAllFish() const { return mFishDefinitions; }
    const std::map<nString, FishID>& getFishIDNames() const { return mFishIdLookup; }

    bool fishExists(FishID id) const { return id < mFishDefinitions.size(); }


private:
    vio::IOManager& mIoManager;

    std::map<nString, FishID> mFishIdLookup; // TODO: StrToken?
    std::vector<FishDef> mFishDefinitions;
};

