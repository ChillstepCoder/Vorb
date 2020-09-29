#pragma once

#include "rendering/Material.h"

class SpriteRepository;
typedef int MaterialID;
DECL_VIO(class Path);
DECL_VIO(class IOManager);

class MaterialManager {
public:
    MaterialManager(vio::IOManager& ioManager, SpriteRepository& spriteRepository);
    ~MaterialManager();

    bool loadMaterial(const vio::Path& filePath);
    const Material* getMaterial(MaterialID id) const;
    const Material* getMaterial(const nString& strId) const;

private:
    std::vector<Material> mMaterials;
    std::unordered_map<nString, MaterialID> mNameToMaterialIDMap;
    vio::IOManager& mIoManager;
    SpriteRepository& mSpriteRepository;
};