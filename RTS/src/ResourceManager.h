#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

class BrushRepository;
class TextureAtlas;
class TextureRepository;
class MaterialManager;
class ParticleSystemManager;
class BuildingDescriptionRepository;
class EntityDefinitionRepository;
class ItemRepository;
class CraftingRepository;
class BusinessRepository;
class ModelRepository;
class AnimationRepository;
class RigRepository;
class AnimMachineRepository;
class SkillRepository;
struct SubTexture;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    void gatherFiles(const vio::Path& folderPath);
    void loadFiles();

    // Resource Accessors
    const SubTexture& getTexture(const nString& textureName) const;

    // TODO: Replace?
    vg::TextureCache& getTextureCache();
    const MaterialManager& getMaterialManager() const { return *mMaterialManager; }
    ParticleSystemManager& getParticleSystemManager() const { return *mParticleSystemManager; }
    BuildingDescriptionRepository& getBuildingRepository() const { return *mBuildingRepository; }
    EntityDefinitionRepository& getEntityDefinitionRepository() const { return *mEntityDefinitionRepository; }
    ItemRepository& getItemRepository() const { return *mItemRepository; }
    BusinessRepository& getBusinessRepository() const { return *mBusinessRepository; }
    AnimationRepository& getAnimationRepository() const { return *mAnimationRepository; }
    RigRepository& getRigRepository() const { return *mRigRepository; }
    AnimMachineRepository& getAnimMachineRepository() const { return *mAnimMachineRepository; }
    ModelRepository& getModelRepository() const { return *mModelRepository; }
    BrushRepository& getBrushRepository() const { return *mBrushRepository; }
    SkillRepository& getSkillRepository() const { return *mSkillRepository; }
    TextureRepository& getTextureRepository() const { return *mTextureRepository; }

    // Hot reload
    void reloadMaterials();

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    
private:
    void gatherRecursive(const vio::Path& folderPath);
    bool loadTiles(const vio::Path& filePath);

    // Tasks
    // TODO: ResourceLoader?
    std::vector<vio::Path> mTextureFiles;
    std::vector<vio::Path> mMaterialFiles;
    std::vector<vio::Path> mTileFiles;
    std::vector<vio::Path> mParticleSystemFiles;
    std::vector<vio::Path> mRoomFiles;
    std::vector<vio::Path> mBuildingFiles;
    std::vector<vio::Path> mEntityFiles;
    std::vector<vio::Path> mItemFiles;
    std::vector<vio::Path> mRecipeFiles;
    std::vector<vio::Path> mBusinessFiles;
    std::vector<vio::Path> mAnimFiles;
    std::vector<vio::Path> mRigFiles;
    std::vector<vio::Path> mAnimMachineFiles;
    std::vector<vio::Path> mModelFiles;
    std::vector<vio::Path> mSkillFiles;

    std::unique_ptr<MaterialManager> mMaterialManager;
    std::unique_ptr<ParticleSystemManager> mParticleSystemManager;
    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;
    std::unique_ptr<EntityDefinitionRepository> mEntityDefinitionRepository;
    std::unique_ptr<ItemRepository> mItemRepository;
    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<AnimationRepository> mAnimationRepository;
    std::unique_ptr<RigRepository> mRigRepository;
    std::unique_ptr<AnimMachineRepository> mAnimMachineRepository;
    std::unique_ptr<ModelRepository> mModelRepository;
    std::unique_ptr<BrushRepository> mBrushRepository;
    std::unique_ptr<SkillRepository> mSkillRepository;
    std::unique_ptr<vg::TextureCache> mTextureCache;
    std::unique_ptr<TextureRepository> mTextureRepository;

    std::unique_ptr<vio::IOManager> mIoManager;

    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

