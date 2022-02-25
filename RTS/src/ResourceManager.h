#pragma once

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

class BrushRepository;
class SpriteRepository;
class TextureAtlas;
class MaterialManager;
class ParticleSystemManager;
class BuildingDescriptionRepository;
class EntityDefinitionRepository;
class ItemRepository;
class CraftingRepository;
class BusinessRepository;
class CharacterModelRepository;
class ModelRepository;
struct SpriteData;

// Loads and manages textures, tiles, and other resources
// TODO: ResourceLoader?
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    void gatherFiles(const vio::Path& folderPath);
    void loadFiles();

    // Resource Accessors
    const SpriteData& getSprite(const std::string& spriteName) const;

    SpriteRepository& getSpriteRepository() { return *mSpriteRepository; }
    // TODO: Replace?
    vg::TextureCache& getTextureCache();
    const TextureAtlas& getTextureAtlas() const;
    const MaterialManager& getMaterialManager() const { return *mMaterialManager; }
    ParticleSystemManager& getParticleSystemManager() const { return *mParticleSystemManager; }
    BuildingDescriptionRepository& getBuildingRepository() const { return *mBuildingRepository; }
    EntityDefinitionRepository& getEntityDefinitionRepository() const { return *mEntityDefinitionRepository; }
    ItemRepository& getItemRepository() const { return *mItemRepository; }
    BusinessRepository& getBusinessRepository() const { return *mBusinessRepository; }
    CharacterModelRepository& getCharacterModelRepository() const { return *mCharacterModelRepository; }
    ModelRepository& getModelRepository() const { return *mModelRepository; }
    BrushRepository& getBrushRepository() const { return *mBrushRepository; }

    // Hot reload
    void reloadMaterials();

    bool hasLoadedResources() const { return mHasLoadedResources; }

    void generateNormalMaps();
    void writeDebugAtlas() const;
    
private:
    void gatherRecursive(const vio::Path& folderPath);
    bool loadTiles(const vio::Path& filePath);

    // Tasks
    // TODO: ResourceLoader
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
    std::vector<vio::Path> mModelFiles;

    std::unique_ptr<SpriteRepository> mSpriteRepository;
    std::unique_ptr<MaterialManager> mMaterialManager;
    std::unique_ptr<ParticleSystemManager> mParticleSystemManager;
    std::unique_ptr<BuildingDescriptionRepository> mBuildingRepository;
    std::unique_ptr<EntityDefinitionRepository> mEntityDefinitionRepository;
    std::unique_ptr<ItemRepository> mItemRepository;
    std::unique_ptr<CraftingRepository> mCraftingRepository;
    std::unique_ptr<BusinessRepository> mBusinessRepository;
    std::unique_ptr<CharacterModelRepository> mCharacterModelRepository;
    std::unique_ptr<ModelRepository> mModelRepository;
    std::unique_ptr<BrushRepository> mBrushRepository;
    std::unique_ptr<vg::TextureCache> mTextureCache;

    std::unique_ptr<vio::IOManager> mIoManager;

    bool mHasLoadedResources = false;
    bool mHasGathered = false;
};

