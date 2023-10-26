#include "stdafx.h"
#include "resources/ResourceManager.h"
#include "resources/AssetLoader.h"

#include "rendering/RenderContext.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/ShaderLoader.h"
#include "city/Building.h"
#include "city/BuildingDescriptionRepository.h"
#include "ecs/EntityDefinitionRepository.h"
#include "item/ItemRepository.h"
#include "crafting/CraftingRepository.h"
#include "ecs/business/BusinessRepository.h"
#include "resources/EffectRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "resources/SkillRepository.h"
#include "resources/TextureRepository.h"
#include "resources/CubemapRepository.h"
#include "resources/TileRepository.h"
#include "resources/TileGrassRepository.h"
#include "resources/FontRepository.h"
#include "resources/FishRepository.h"
#include "resources/ParticleSystemRepository.h"
#include "physics/CollisionShapeRepository.h"
#include "editor/BrushRepository.h"
#include "editor/EditorResources.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/IO.h>
#include <vorb/io/FileOps.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>

ResourceManager* sInstance = nullptr;

struct ShaderData {
    nString vert;
    nString frag;
};
KEG_TYPE_DECL(ShaderData);
KEG_TYPE_DEF_SAME_NAME(ShaderData, kt) {
    kt.addValue("vert", keg::Value::basic(offsetof(ShaderData, vert), keg::BasicType::STRING));
    kt.addValue("frag", keg::Value::basic(offsetof(ShaderData, frag), keg::BasicType::STRING));
}

#define REGISTER_ASSET_REPO(RepoClass, AType, ...) \
    RepoClass::initInstance(*mIoManager, __VA_ARGS__); \
    mAssetRepositories[(size_t)AType] = &RepoClass::get(); \
    RepoClass::get().init(); \
    mExtensionToAssetRepository[RepoClass::get().getAssetExtension()] = &RepoClass::get();

ResourceManager::ResourceManager() {
    assert(!sInstance);
    sInstance = this;
    
    mIoManager = std::make_unique<vio::IOManager>();
    AssetLoader::initInstance();

    mCollisionShapeRepository = std::make_unique<CollisionShapeRepository>();
    
    mAssetRepositories.resize(e_count(AssetType));
    mExtensionToAssetRepository.reserve(e_count(AssetType));
    REGISTER_ASSET_REPO(TileRepository, AssetType::Tile, *mCollisionShapeRepository);
    REGISTER_ASSET_REPO(ParticleSystemRepository, AssetType::ParticleSystem);
    REGISTER_ASSET_REPO(EffectRepository, AssetType::Effect);
    REGISTER_ASSET_REPO(TextureRepository, AssetType::Texture);
    REGISTER_ASSET_REPO(CubemapRepository, AssetType::Cubemap);
    REGISTER_ASSET_REPO(BrushRepository, AssetType::Brush);
    REGISTER_ASSET_REPO(MaterialRepository, AssetType::Material);
    REGISTER_ASSET_REPO(RigRepository, AssetType::Rig);
    REGISTER_ASSET_REPO(AnimationRepository, AssetType::Animation);
    REGISTER_ASSET_REPO(AnimMachineRepository, AssetType::AnimMachine);
    REGISTER_ASSET_REPO(ModelRepository, AssetType::Model);
    REGISTER_ASSET_REPO(SkillRepository, AssetType::Skill);
    REGISTER_ASSET_REPO(ItemRepository, AssetType::Item);
    REGISTER_ASSET_REPO(FishRepository, AssetType::Fish);
    REGISTER_ASSET_REPO(MaterialShaderRepository, AssetType::MaterialShader);
    REGISTER_ASSET_REPO(TileGrassRepository, AssetType::TileGrass);
    static_assert(e_count(AssetType) == 16);

    // Add other extensions
    mExtensionToAssetRepository[CStrToken("comp")] = &MaterialShaderRepository::get();

    mBuildingRepository = std::make_unique<BuildingDescriptionRepository>(*mIoManager);
    mEntityDefinitionRepository = std::make_unique<EntityDefinitionRepository>(*mIoManager);
    mCraftingRepository = std::make_unique<CraftingRepository>(*mIoManager);
    mBusinessRepository = std::make_unique<BusinessRepository>(*mIoManager);
    mFontRepository = std::make_unique<FontRepository>();
}

ResourceManager::~ResourceManager() {
    EditorResources::freeAllResources();
    sInstance = nullptr;
}

ResourceManager& ResourceManager::get() {
    assert(sInstance);
    return *sInstance;
}

bool fileHasExtension(const vio::Path& filePath, const std::string& extension) {
    size_t length = filePath.getString().size();
    if (filePath.getString().size() <= extension.size()) {
        return false;
    }
    return strcmp(filePath.getString().c_str() + (length - extension.size()), extension.c_str()) == 0;
}

void ResourceManager::setResourceRoot(const vio::Path& folderPath) {
    if (!mIoManager->resolvePath(folderPath, mResourceRoot)) {
        pError("Could not resolve /data/ folder. Try verifying game files");
    }
}

void ResourceManager::gatherFiles() {

    PreciseTimer timer;
    
    mRoomFiles.clear();
    mBuildingFiles.clear();
    mEntityFiles.clear();
    mRecipeFiles.clear();
    mBusinessFiles.clear();
    mFontFiles.clear();

    assert(mResourceRoot.isValid());

    gatherRecursive(mResourceRoot);

    // Any needed post processing
    for (auto& assetRepo : mAssetRepositories) {
        assetRepo->onAllAssetTypesRegistered();
    }

    preloadFiles();

    mHasGathered = true;

    LOG_INFO("Gathered files in {:.4} ms", timer.stop());
}

void ResourceManager::loadFiles() {
    assert(mHasGathered);

    PreciseTimer totalTimer;


    // Load recipe definitions
    {
        ScopedTimer timer("Recipe load");
        for (auto&& entry : mRecipeFiles) {
            mCraftingRepository->loadRecipeFile(entry);
        }
    }

    {
        // Set default material
        ParticleSystemRepository& repo = ParticleSystemRepository::get();
        repo.setDefaultMaterialID(MaterialRepository::get().getMaterialId(CStrToken("particle_v0"/*"soft_particle"*/)));
    }

    // Load Rooms
    {
        ScopedTimer timer("City load");
        for (auto&& entry : mRoomFiles) {
            mBuildingRepository->loadRoomDescriptionFile(entry);
        }

        // Load Buildings
        for (auto&& entry : mBuildingFiles) {
            mBuildingRepository->loadBuildingDescriptionFile(entry);
        }

        // Load business definitions
        for (auto&& entry : mBusinessFiles) {
            mBusinessRepository->loadBusinessFile(entry);
        }
    }

    // Load entity definitions
    {
        ScopedTimer timer("Entity load");
        for (auto&& entry : mEntityFiles) {
            mEntityDefinitionRepository->loadEntityDefinitionFile(entry);
        }
    }

    // Load font definitions
    {
        ScopedTimer timer("Font load");
        for (auto&& entry : mFontFiles) {
            mFontRepository->loadFont(entry);
        }
    }

    mHasLoadedResources = true;
    LOG_INFO("Loaded resources in {} s", totalTimer.stop() / 1000.0);
}

void ResourceManager::reloadMaterials() {

    if (!mHasLoadedResources) {
        return;
    }

    LOG_DEBUG("Reloading materials...");

    ShaderLoader::clearAllCachedPrograms();
    vg::ShaderManager::disposeAllPrograms();
    AssetHandleBundle assets = MaterialShaderRepository::get().reloadAllLoadedAssets();
    AssetLoader& loader = AssetLoader::getInstance();
    while (!assets.areAllAssetsLoaded()) {
        loader.update();
        Sleep(1);
        RenderContext::getInstance().updateRenderThreadProcs();
    }

    LOG_DEBUG("...done");
}

void ResourceManager::generateNormalMaps() {
    //TODO: This not do anything!
    glTextureBarrier();
}

void ResourceManager::addAssetToBundle(AssetHandleBundle& bundle, StrToken assetName, AssetType assetType) {
    bundle.addAssetHandle(mAssetRepositories[e_cast(assetType)]->getAssetHandleBase(assetName));
}

AssetType ResourceManager::getAssetTypeForFilePath(const std::filesystem::path& path) {
    StrToken extension(Utils::getExtension(path.string()));
    IAssetRepositoryBase* repo = tryGetAssetRepositoryForFileExtension(extension);
    if (!repo) {
        return AssetType::NONE;
    }
    return repo->getAssetType();
}

IAssetRepositoryBase* ResourceManager::tryGetAssetRepositoryForFileExtension(StrToken extension) const {
    auto&& it = mExtensionToAssetRepository.find(extension);
    if (it == mExtensionToAssetRepository.end()) {
        return nullptr;
    }
    return it->second;
}

AssetDescriptor ResourceManager::registerOrGetRegisteredAsset(const std::filesystem::path& path) {
    AssetType type = Services::ResourceManager::ref().getAssetTypeForFilePath(path);
    if (type == AssetType::NONE) {
        return AssetDescriptor();
    }
    IAssetRepositoryBase& repo = Services::ResourceManager::ref().getAssetRepository(type);
    nString pathString = path.string();
    pathString = Utils::getFilenameNoExtension(pathString);
    StrToken assetName(pathString.data(), pathString.size());
    if (!repo.isAssetRegistered(assetName)) {
        repo.registerAssetPath(vio::Path(path));
    }
    return AssetDescriptor{ .id=repo.getAssetID(assetName), .assetType=type };
}

AssetMetadata ResourceManager::getAssetMetadata(AssetDescriptor desc) {
    assert(desc.isValid());
    IAssetRepositoryBase& repo = Services::ResourceManager::ref().getAssetRepository(desc.assetType);
    return repo.getMetadata(desc.id);
}

AssetMetadata ResourceManager::tryGetAssetMetadataForPath(const std::filesystem::path& path) {
    AssetType type = Services::ResourceManager::ref().getAssetTypeForFilePath(path);
    if (type == AssetType::NONE) {
        return AssetMetadata();
    }

    StrToken assetName(Utils::getFilenameNoExtension(path.string()));
    IAssetRepositoryBase& baseRepo = getAssetRepository(type);
    return baseRepo.tryGetMetadata(assetName);
}

void ResourceManager::gatherRecursive(const vio::Path& folderPath)
{
    vio::Directory directory;
    if (!folderPath.asDirectory(&directory)) {
        LOG_CRITICAL("{} Could not be resolved, resource manager cannot find resources", folderPath.getString());
        pError("Could not resolve data root path");
    }

    vio::DirectoryEntries entries;
    if (!directory.appendEntries(entries)) {
        // Empty directory
        return;
    }

    // TODO: This can all be optimized with getFileExtension() and then a string map lookup, std::map<std::string, std::function<void(const Vio::Path& entry)>
    for (auto&& entry : entries) {
        // Recurse
        // TODO: Map lookup for minor optimization? Sort by extension? idk, (im starting to hate this)
        if (entry.isDirectory()) {
            gatherRecursive(entry);
        }
        else if (fileHasExtension(entry, ".png")) {
            if (vio::containsSubpath(entry, "_brushes")) {
                BrushRepository::get().registerAssetPath(entry);
            }
            TextureRepository::get().registerAssetPath(entry);
            
        }
        else if (fileHasExtension(entry, ".cube")) {
            CubemapRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".room")) {
            mRoomFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".bldg")) {
            mBuildingFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".tile")) {
            TileRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".prog")) {
            MaterialShaderRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".material")) {
            MaterialRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".comp")) {
            MaterialShaderRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".vert")) {
            ShaderLoader::registerVertexShaderPath(entry.getLeaf(), entry);
        }
        else if (fileHasExtension(entry, ".frag")) {
            ShaderLoader::registerFragmentShaderPath(entry.getLeaf(), entry);
        }
        else if (fileHasExtension(entry, ".geom")) {
            ShaderLoader::registerGeometryShaderPath(entry.getLeaf(), entry);
        }
        else if (fileHasExtension(entry, ".tcs")) {
            ShaderLoader::registerTessControlShaderPath(entry.getLeaf(), entry);
        }
        else if (fileHasExtension(entry, ".tes")) {
            ShaderLoader::registerTessEvalShaderPath(entry.getLeaf(), entry);
        }
        else if (fileHasExtension(entry, ".psys")) {
            ParticleSystemRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".effect")) {
            EffectRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".ent")) {
            mEntityFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".recipe")) {
            mRecipeFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".item")) {
            ItemRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".fish")) {
            FishRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".business")) {
            mBusinessFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".model")) {
            ModelRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".rig")) {
            RigRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".machine")) {
            AnimMachineRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".skill")) {
            SkillRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".anim")) {
            AnimationRepository::get().registerAssetPath(entry);
        }
        else if (fileHasExtension(entry, ".ttf")) {
            mFontFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".grass")) {
            TileGrassRepository::get().registerAssetPath(entry);
        }
    }
}

void ResourceManager::preloadFiles() {
    vio::Path preloadPath = mResourceRoot / vio::Path("assets.preload");
    nString data;
    if (!mIoManager->readFileToString(preloadPath, data)) {
        panic("assets.preload does not exist");
    }
    ryml::Tree tree = YmlSerializer::parseFileData(data);
    ryml::ConstNodeRef root = tree.crootref();
    ryml::ConstNodeRef startupSeq = root["startup"];
    ryml::ConstNodeRef preloadSeq = root["preload"];

    StrToken assetName;
    AssetType assetType;
    for (ryml::ConstNodeRef child : startupSeq.children()) {
        if (child.num_children() != 2) {
            panic("Malformed entry in assets.preload::startup");
        }
        child.child(0) >> assetName;
        child.child(1) >> assetType;
        addAssetToBundle(mPreloadAssetsBundle, assetName, assetType);
    }

    LOG_INFO("Loading startup assets...");
    AssetLoader& loader = AssetLoader::getInstance();
    while (!mPreloadAssetsBundle.areAllAssetsLoaded()) {
        loader.update();
        Sleep(1);
        RenderContext::getInstance().updateRenderThreadProcs();
    }
    LOG_INFO("Done");


    LOG_INFO("Loading editor assets...");
    // Editor resources
    EditorResources::loadAllResources();
    while (!EditorResources::isFullyLoaded()) {
        loader.update();
        Sleep(1);
        RenderContext::getInstance().updateRenderThreadProcs();
    }
    LOG_INFO("Done");

    constexpr StrToken wildcard = CStrToken("*");
    for (ryml::ConstNodeRef child : preloadSeq.children()) {
        if (child.num_children() != 2) {
            panic("Malformed entry in assets.preload::startup");
        }
        child.child(0) >> assetName;
        child.child(1) >> assetType;
        if (assetName == wildcard) {
            for (size_t i = 0; i < mAssetRepositories[e_cast(assetType)]->getNumRegisteredAssets(); ++i) {
                if (!mPreloadAssetsBundle.hasAssetHandle(AssetDescriptor{ .id = (AssetID)i,.assetType = assetType })) {
                    mPreloadAssetsBundle.addAssetHandle(mAssetRepositories[e_cast(assetType)]->getAssetHandleBase(i));
                }
            }
        }
        else {
            addAssetToBundle(mPreloadAssetsBundle, assetName, assetType);
        }
    }

}
