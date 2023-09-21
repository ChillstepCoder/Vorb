#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

#include "IAssetRepository.h"

class AnimationRepository;
class AnimMachineRepository;
class BrushRepository;
class BuildingDescriptionRepository;
class BusinessRepository;
class CollisionShapeRepository;
class CraftingRepository;
class EntityDefinitionRepository;
class FontRepository;
class ParticleSystemRepository;
class MaterialRepository;
class MaterialShaderManager;
class ModelRepository;
class RigRepository;
class SkillRepository;
class TextureRepository;
class TileGrassRepository;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    void setResourceRoot(const vio::Path& folderPath);

    void gatherFiles();
    void loadFiles();

    // TODO: These are all obsolete? Since each is a singleton?
    MaterialShaderManager& getMaterialShaderManager() const { return *mMaterialManager; }
    BuildingDescriptionRepository& getBuildingDescriptionRepository() const { return *mBuildingRepository; }
    EntityDefinitionRepository& getEntityDefinitionRepository() const { return *mEntityDefinitionRepository; }
    BusinessRepository& getBusinessRepository() const { return *mBusinessRepository; }
    FontRepository& getFontRepository() const { return *mFontRepository; }
    CollisionShapeRepository& getCollisionShapeRepository() const { return *mCollisionShapeRepository; }
    TileGrassRepository& getTileGrassRepository() const { return *mTileGrassRepository; }
    vio::IOManager& getIoManager() const { return *mIoManager; }

    // Hot reload
    void reloadMaterials();

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    
    const vio::Path& getResourceRoot() const { return mResourceRoot; }

    void addAssetToBundle(AssetHandleBundle& bundle, StrToken assetName, AssetType assetType);

private:
    void gatherRecursive(const vio::Path& folderPath);
    void preloadFiles();

    // Tasks
    // TODO: ResourceLoader?
    std::vector<vio::Path> mMaterialShaderFiles;
    std::vector<vio::Path> mComputeFiles;
    std::vector<vio::Path> mTileFiles;
    std::vector<vio::Path> mTileGrassFiles;
    std::vector<vio::Path> mRoomFiles;
    std::vector<vio::Path> mBuildingFiles;
    std::vector<vio::Path> mEntityFiles;
    std::vector<vio::Path> mRecipeFiles;
    std::vector<vio::Path> mBusinessFiles;
    std::vector<vio::Path> mFontFiles;

    std::unique_ptr<MaterialShaderManager> mMaterialManager;
    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;
    std::unique_ptr<EntityDefinitionRepository> mEntityDefinitionRepository;
    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<FontRepository> mFontRepository;
    std::unique_ptr<CollisionShapeRepository> mCollisionShapeRepository;
    std::unique_ptr<TileGrassRepository> mTileGrassRepository;

    std::vector<IAssetRepositoryBase*> mAssetRepositories;
    AssetHandleBundle mPreloadAssetsBundle;

    // TODO: Replace with std::filesystem?
    std::unique_ptr<vio::IOManager> mIoManager;

    vio::Path mResourceRoot;
    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

