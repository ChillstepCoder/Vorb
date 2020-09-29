#pragma once

DECL_VIO(class IOManager);

#include <Vorb/io/Path.h>
#include "world/Tile.h"

#include "rendering/SpriteRepository.h"

class MaterialManager;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    void gatherFiles(const vio::Path& folderPath);
    void loadFiles();

    // Resource Accessors
    const SpriteData& getSprite(const std::string& spriteName);

    SpriteRepository& getSpriteRepository() { return *mSpriteRepository; }
    // TODO: Replace?
    vg::TextureCache& getTextureCache() { return mSpriteRepository->getTextureCache(); }
    const TextureAtlas& getTextureAtlas() const { return mSpriteRepository->getTextureAtlas(); }
    const MaterialManager& getMaterialManager() const { return *mMaterialManager; }
    bool hasLoadedResources() const { return mHasLoadedResources; }

    void writeDebugAtlas() const;
    
private:
    bool loadTiles(const vio::Path& filePath);
    bool loadShader(const vio::Path& filePath);

    // Tasks
    // TODO: ResourceLoader
    std::vector<vio::Path> mShaderFiles;
    std::vector<vio::Path> mTextureFiles;
    std::vector<vio::Path> mMaterialFiles;
    std::vector<vio::Path> mTileFiles;

    std::unique_ptr<SpriteRepository> mSpriteRepository;
    std::unique_ptr<MaterialManager> mMaterialManager;

    std::unique_ptr<vio::IOManager> mIoManager;

    TileID mIdGenerator = 0; // Generates IDs

    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

