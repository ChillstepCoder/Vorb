#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

class AnimationRepository;
class AnimMachineRepository;
class BrushRepository;
class BuildingDescriptionRepository;
class BusinessRepository;
class CollisionShapeRepository;
class CraftingRepository;
class EntityDefinitionRepository;
class FontRepository;
class ItemRepository;
class FishRepository;
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

    MaterialShaderManager& getMaterialShaderManager() const { return *mMaterialManager; }
    MaterialRepository& getMaterialRepository() const { return *mMaterialRepository; }
    BuildingDescriptionRepository& getBuildingDescriptionRepository() const { return *mBuildingRepository; }
    EntityDefinitionRepository& getEntityDefinitionRepository() const { return *mEntityDefinitionRepository; }
    ItemRepository& getItemRepository() const { return *mItemRepository; }
    FishRepository& getFishRepository() const { return *mFishRepository; }
    BusinessRepository& getBusinessRepository() const { return *mBusinessRepository; }
    AnimationRepository& getAnimationRepository() const { return *mAnimationRepository; }
    RigRepository& getRigRepository() const { return *mRigRepository; }
    AnimMachineRepository& getAnimMachineRepository() const { return *mAnimMachineRepository; }
    ModelRepository& getModelRepository() const { return *mModelRepository; }
    BrushRepository& getBrushRepository() const { return *mBrushRepository; }
    SkillRepository& getSkillRepository() const { return *mSkillRepository; }
    TextureRepository& getTextureRepository() const { return *mTextureRepository; }
    FontRepository& getFontRepository() const { return *mFontRepository; }
    CollisionShapeRepository& getCollisionShapeRepository() const { return *mCollisionShapeRepository; }
    TileGrassRepository& getTileGrassRepository() const { return *mTileGrassRepository; }
    vio::IOManager& getIoManager() const { return *mIoManager; }

    // Hot reload
    void reloadMaterials();

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    
    const vio::Path& getResourceRoot() const { return mResourceRoot; }
private:
    void gatherRecursive(const vio::Path& folderPath);

    // Tasks
    // TODO: ResourceLoader?
    std::vector<vio::Path> mBrushFiles;
    std::vector<vio::Path> mCubemapFiles;
    std::vector<vio::Path> mMaterialFiles;
    std::vector<vio::Path> mMaterialShaderFiles;
    std::vector<vio::Path> mComputeFiles;
    std::vector<vio::Path> mTileFiles;
    std::vector<vio::Path> mTileGrassFiles;
    std::vector<vio::Path> mParticleSystemFiles;
    std::vector<vio::Path> mRoomFiles;
    std::vector<vio::Path> mBuildingFiles;
    std::vector<vio::Path> mEntityFiles;
    std::vector<vio::Path> mItemFiles;
    std::vector<vio::Path> mFishFiles;
    std::vector<vio::Path> mRecipeFiles;
    std::vector<vio::Path> mBusinessFiles;
    std::vector<vio::Path> mAnimFiles;
    std::vector<vio::Path> mRigFiles;
    std::vector<vio::Path> mAnimMachineFiles;
    std::vector<vio::Path> mModelFiles;
    std::vector<vio::Path> mSkillFiles;
    std::vector<vio::Path> mFontFiles;

    std::unique_ptr<MaterialShaderManager> mMaterialManager;
    std::unique_ptr<MaterialRepository> mMaterialRepository;
    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;
    std::unique_ptr<EntityDefinitionRepository> mEntityDefinitionRepository;
    std::unique_ptr<ItemRepository> mItemRepository;
    std::unique_ptr<FishRepository> mFishRepository;
    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<AnimationRepository> mAnimationRepository;
    std::unique_ptr<RigRepository> mRigRepository;
    std::unique_ptr<AnimMachineRepository> mAnimMachineRepository;
    std::unique_ptr<ModelRepository> mModelRepository;
    std::unique_ptr<BrushRepository> mBrushRepository;
    std::unique_ptr<SkillRepository> mSkillRepository;
    std::unique_ptr<TextureRepository> mTextureRepository;
    std::unique_ptr<FontRepository> mFontRepository;
    std::unique_ptr<CollisionShapeRepository> mCollisionShapeRepository;
    std::unique_ptr<TileGrassRepository> mTileGrassRepository;

    // TODO: Replace with std::filesystem?
    std::unique_ptr<vio::IOManager> mIoManager;

    vio::Path mResourceRoot;
    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

