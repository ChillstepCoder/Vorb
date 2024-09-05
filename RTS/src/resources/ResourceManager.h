#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

#include "IAssetRepository.h"

class AnimationRepository;
class AnimMachineRepository;
class BiomeRepository;
class BrushRepository;
class BuildingRepository;
class BusinessRepository;
class CollisionShapeRepository;
class CraftingRepository;
class EntityRepository;
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
    StrToken getAssetExtension(AssetType type) {
        return getAssetRepository(type).getAssetExtension();
    }
    AssetDescriptor registerOrGetRegisteredAsset(const std::filesystem::path& path);
    AssetMetadata getAssetMetadata(AssetDescriptor desc);
    AssetMetadata tryGetAssetMetadataForPath(const std::filesystem::path& path);

    template <IsAssetType T>
    static AssetHandlePtr<T> getAssetHandle(AssetID id) {
        IAssetRepositoryBase& repo = get().getAssetRepository(T::ASSET_TYPE);
        return static_unique_pointer_cast<AssetHandle<T>>(repo.getAssetHandleBase(id));
    }
    template <IsAssetType T>
    static AssetHandlePtr<T> getAssetHandle(StrToken assetName) {
        IAssetRepositoryBase& repo = get().getAssetRepository(T::ASSET_TYPE);
        return static_unique_pointer_cast<AssetHandle<T>>(repo.getAssetHandleBase(assetName));
    }

    static void reloadAsset(AssetDescriptor desc) {
        IAssetRepositoryBase& repo = get().getAssetRepository(desc.assetType);
        repo.reloadAsset(desc.id);
    }
    static void renameAsset(AssetDescriptor prevDesc, const std::filesystem::path& newPath) {
        IAssetRepositoryBase& repo = get().getAssetRepository(prevDesc.assetType);
        repo.renameAsset(prevDesc, newPath);
    }
    static AssetID getAssetID(StrToken assetName, AssetType assetType) {
        IAssetRepositoryBase& repo = get().getAssetRepository(assetType);
        return repo.getAssetID(assetName);
    }
private:
    void gatherRecursive(const vio::Path& folderPath);
    void preloadFiles();
    void preloadBundleInternal(AssetHandleBundle& bundle);

    // Tasks
    // TODO: ResourceLoader?
    std::vector<vio::Path> mRecipeFiles;
    std::vector<vio::Path> mBusinessFiles;
    std::vector<vio::Path> mFontFiles;

    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<FontRepository> mFontRepository;
    std::unique_ptr<CollisionShapeRepository> mCollisionShapeRepository;

    std::vector<IAssetRepositoryBase*> mAssetRepositories;
    UnorderedFlatMap<StrToken, IAssetRepositoryBase*> mExtensionToAssetRepository;
    AssetHandleBundle mPreloadAssetsBundle;
    AssetHandleBundle mRigsAndAnimMachinesHandles;
    AssetHandleBasePtr mLoadscreenTextureHandle;

    // TODO: Replace with std::filesystem?
    std::unique_ptr<vio::IOManager> mIoManager;

    vio::Path mResourceRoot;
    vio::Path mCacheRoot;
    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

