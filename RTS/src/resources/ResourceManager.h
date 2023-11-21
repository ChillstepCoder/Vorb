#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

#include "IAssetRepository.h"
#include "filesystem/FileSystem.h"

class AnimationRepository;
class AnimMachineRepository;
class BiomeRepository;
class BrushRepository;
class BuildingDescriptionRepository;
class BusinessRepository;
class CollisionShapeRepository;
class CraftingRepository;
class EntityDefinitionRepository;
class FontRepository;
class ParticleSystemRepository;
class ModelRepository;
class RigRepository;
class SkillRepository;
class TextureRepository;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    static ResourceManager& get();

    void setResourceRoot(const vio::Path& resourceRoot, const vio::Path& cacheRoot);

    void gatherFiles();
    void loadFiles();

    BuildingDescriptionRepository& getBuildingDescriptionRepository() const { return *mBuildingRepository; }
    EntityDefinitionRepository& getEntityDefinitionRepository() const { return *mEntityDefinitionRepository; }
    BusinessRepository& getBusinessRepository() const { return *mBusinessRepository; }
    FontRepository& getFontRepository() const { return *mFontRepository; }
    CollisionShapeRepository& getCollisionShapeRepository() const { return *mCollisionShapeRepository; }
    vio::IOManager& getIoManager() const { return *mIoManager; }

    // Hot reload
    void reloadMaterials();

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    
    const vio::Path& getResourceRoot() const { return mResourceRoot; }
    const vio::Path& getCacheRoot() const { return mCacheRoot; }

    void addAssetToBundle(AssetHandleBundle& bundle, StrToken assetName, AssetType assetType);

    AssetType getAssetTypeForFilePath(const std::filesystem::path& path);
    IAssetRepositoryBase* tryGetAssetRepositoryForFileExtension(StrToken extension) const;
    IAssetRepositoryBase& getAssetRepository(AssetType type) {
        assert((size_t)type < mAssetRepositories.size());
        return *mAssetRepositories[e_cast(type)];
    }
    AssetDescriptor registerOrGetRegisteredAsset(const std::filesystem::path& path);
    AssetMetadata getAssetMetadata(AssetDescriptor desc);
    AssetMetadata tryGetAssetMetadataForPath(const std::filesystem::path& path);

private:
    void gatherRecursive(const vio::Path& folderPath);
    void preloadFiles();

    // Tasks
    // TODO: ResourceLoader?
    std::vector<vio::Path> mRoomFiles;
    std::vector<vio::Path> mBuildingFiles;
    std::vector<vio::Path> mEntityFiles;
    std::vector<vio::Path> mRecipeFiles;
    std::vector<vio::Path> mBusinessFiles;
    std::vector<vio::Path> mFontFiles;

    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;
    std::unique_ptr<EntityDefinitionRepository> mEntityDefinitionRepository;
    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<FontRepository> mFontRepository;
    std::unique_ptr<CollisionShapeRepository> mCollisionShapeRepository;

    std::vector<IAssetRepositoryBase*> mAssetRepositories;
    std::unordered_map<StrToken, IAssetRepositoryBase*> mExtensionToAssetRepository;
    AssetHandleBundle mPreloadAssetsBundle;

    // TODO: Replace with std::filesystem?
    std::unique_ptr<vio::IOManager> mIoManager;

    vio::Path mResourceRoot;
    vio::Path mCacheRoot;
    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

