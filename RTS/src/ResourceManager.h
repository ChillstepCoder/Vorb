#pragma once

DECL_VIO(class IOManager);

#include <Vorb/io/Path.h>
#include "world/Tile.h"

#include "rendering/SpriteRepository.h"


class MaterialManager;
class ParticleSystemManager;
class BuildingDescriptionRepository;

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
    ParticleSystemManager& getParticleSystemManager() const { return *mParticleSystemManager; }
    BuildingDescriptionRepository& getBuildingRepository() const { return *mBuildingRepository; }

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    void writeDebugAtlas() const;
    
private:
    bool loadTiles(const vio::Path& filePath);

    // Tasks
    // TODO: ResourceLoader
    std::vector<vio::Path> mTextureFiles;
    std::vector<vio::Path> mMaterialFiles;
    std::vector<vio::Path> mTileFiles;
    std::vector<vio::Path> mParticleSystemFiles;
    std::vector<vio::Path> mRoomFiles;
    std::vector<vio::Path> mBuildingFiles;

    std::unique_ptr<SpriteRepository> mSpriteRepository;
    std::unique_ptr<MaterialManager> mMaterialManager;
    std::unique_ptr<ParticleSystemManager> mParticleSystemManager;
    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;

    std::unique_ptr<vio::IOManager> mIoManager;

    TileID mIdGenerator = 0; // Generates IDs

    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

