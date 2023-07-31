#include "stdafx.h"
#include "resources/ResourceManager.h"

#include "rendering/MaterialShaderManager.h"
#include "rendering/ShaderLoader.h"
#include "city/Building.h"
#include "city/BuildingDescriptionRepository.h"
#include "ecs/EntityDefinitionRepository.h"
#include "item/ItemRepository.h"
#include "crafting/CraftingRepository.h"
#include "ecs/business/BusinessRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "resources/SkillRepository.h"
#include "resources/TextureRepository.h"
#include "resources/TileRepository.h"
#include "resources/TileGrassRepository.h"
#include "resources/FontRepository.h"
#include "resources/FishRepository.h"
#include "resources/ParticleSystemRepository.h"
#include "physics/CollisionShapeRepository.h"
#include "editor/BrushRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/IO.h>
#include <vorb/io/FileOps.h>
#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/GLProgram.h>

struct ShaderData {
    nString vert;
    nString frag;
};
KEG_TYPE_DECL(ShaderData);
KEG_TYPE_DEF_SAME_NAME(ShaderData, kt) {
    kt.addValue("vert", keg::Value::basic(offsetof(ShaderData, vert), keg::BasicType::STRING));
    kt.addValue("frag", keg::Value::basic(offsetof(ShaderData, frag), keg::BasicType::STRING));
}

ResourceManager::ResourceManager() {
    
    mIoManager = std::make_unique<vio::IOManager>();

    mTextureRepository = std::make_unique<TextureRepository>(*mIoManager);
    mMaterialManager = std::make_unique<MaterialShaderManager>(*mIoManager, *mTextureRepository);
    mMaterialRepository = std::make_unique<MaterialRepository>(*mIoManager);
    mBuildingRepository = std::make_unique<BuildingDescriptionRepository>(*mIoManager);
    mEntityDefinitionRepository = std::make_unique<EntityDefinitionRepository>(*mIoManager);
    mItemRepository = std::make_unique<ItemRepository>(*mIoManager);
    mFishRepository = std::make_unique<FishRepository>(*mIoManager);
    mParticleSystemRepository = std::make_unique<ParticleSystemRepository>(*mIoManager, *mMaterialRepository);
    mCraftingRepository = std::make_unique<CraftingRepository>(*mIoManager);
    mBusinessRepository = std::make_unique<BusinessRepository>(*mIoManager, *mItemRepository);
    mAnimationRepository = std::make_unique<AnimationRepository>();
    mRigRepository = std::make_unique<RigRepository>(*mIoManager);
    mAnimMachineRepository = std::make_unique<AnimMachineRepository>(*mIoManager, *mRigRepository);
    mModelRepository = std::make_unique<ModelRepository>(*mIoManager, *mRigRepository);
    mBrushRepository = std::make_unique<BrushRepository>(*mIoManager);
    mSkillRepository = std::make_unique<SkillRepository>(*mIoManager);
    mFontRepository = std::make_unique<FontRepository>();
    mCollisionShapeRepository = std::make_unique<CollisionShapeRepository>();
    mTileGrassRepository = std::make_unique<TileGrassRepository>();
}

ResourceManager::~ResourceManager() {

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

    // Make sure we clear all vectors each gather
    mBrushFiles.clear();
    mMaterialShaderFiles.clear();
    mMaterialFiles.clear();
    mTileFiles.clear();
    mTileGrassFiles.clear();
    mParticleSystemFiles.clear();
    mRoomFiles.clear();
    mBuildingFiles.clear();
    mEntityFiles.clear();
    mItemFiles.clear();
    mFishFiles.clear();
    mRecipeFiles.clear();
    mBusinessFiles.clear();
    mModelFiles.clear();
    mAnimFiles.clear();
    mRigFiles.clear();
    mAnimMachineFiles.clear();
    mSkillFiles.clear();
    mFontFiles.clear();

    assert(mResourceRoot.isValid());

    gatherRecursive(mResourceRoot);
    mHasGathered = true;

    LOG_INFO("Gathered files in {:.4} ms", timer.stop());
}

void ResourceManager::loadFiles() {
    assert(mHasGathered);

    PreciseTimer totalTimer;

    { // Brushes
        ScopedTimer timer("Brush load");
        for (auto&& entry : mBrushFiles) {
             mBrushRepository->loadBrush(entry, *mTextureRepository);
        }
    }

    // Load Materials
    {
        ScopedTimer timer("Material load");
        for (auto&& entry : mMaterialFiles) {
            mMaterialRepository->loadMaterial(entry, *mTextureRepository);
        };
        mMaterialRepository->uploadMaterialData();
    }

    // Load item definitions
    {
        ScopedTimer timer("Item load");
        for (auto&& entry : mItemFiles) {
            mItemRepository->loadItemFile(entry, *mTextureRepository);
        }
    }

    // Load recipe definitions
    {
        ScopedTimer timer("Recipe load");
        for (auto&& entry : mRecipeFiles) {
            mCraftingRepository->loadRecipeFile(*mItemRepository, entry);
        }
    }

    // Load Material Shaders
    {
        ScopedTimer timer("Material Shader load");
        for (auto&& entry : mMaterialShaderFiles) {
            mMaterialManager->loadMaterialShader(entry);
        };
    }

    // Load Compute
    {
        ScopedTimer timer("Compute load");
        for (auto&& entry : mComputeFiles) {
            mMaterialManager->loadComputeShader(entry);
        };
    }

    { // Load cubemaps
        for (auto&& entry : mCubemapFiles) {
            mTextureRepository->loadCubemap(entry);
        }
    }


    // Load Animations
    {
        ScopedTimer timer("Animation load");
        for (auto&& entry : mAnimFiles) {
            mAnimationRepository->loadAnimFile(entry);
        }
    }

    // Load Rigs
    {
        ScopedTimer timer("Rig load");
        for (auto&& entry : mRigFiles) {
            mRigRepository->loadRigFile(entry, *mAnimationRepository);
        }
    }

    // Load Animation Machines
    {
        ScopedTimer timer("Animation Machine load");
        for (auto&& entry : mAnimMachineFiles) {
            mAnimMachineRepository->loadMachineFile(entry);
        }
    }

    // Load Models
    {
        ScopedTimer timer("Model load");
        for (auto&& entry : mModelFiles) {
            mModelRepository->loadModelFile(entry, *mMaterialRepository, *mAnimMachineRepository);
        }
    }

    // Load Fish
    {
        ScopedTimer timer("Fish load");
        for (auto&& entry : mFishFiles) {
            mFishRepository->loadFishFile(entry, *mModelRepository, *mItemRepository, *mTextureRepository);
        }
    }

    // Load Tiles (Must be done after texture, item and recipes, models)
    {
        ScopedTimer timer("Tile load");
        TileRepository::sTileData.reserve(mTileFiles.size() + 10);
        for (auto&& entry : mTileFiles) {
            // TODO: Tilemanager?
            TileRepository::loadTileFile(*mIoManager, entry, *mMaterialRepository, *mItemRepository, *mModelRepository, *mCollisionShapeRepository);
        }
    }

    // Load grass
    {
        ScopedTimer timer("Grass load");
        for (auto&& entry : mTileGrassFiles) {
            // TODO: Tilemanager?
            mTileGrassRepository->loadGrassFile(*mIoManager, entry, *mMaterialRepository);
        }
    }

    // Load skills
    {
        ScopedTimer timer("Skill load");
        for (auto&& entry : mSkillFiles) {
            mSkillRepository->loadSkillFile(entry, *mAnimationRepository);
        }
    }

    // Load particle Systems
    {
        ScopedTimer timer("Particle load");
     /*   for (auto&& entry : mParticleSystemFiles) {
            mParticleSystemManager->loadParticleSystemData(entry);
        };*/
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
    LOG_INFO("Loaded resources in {:.4} ms", totalTimer.stop());
}

void ResourceManager::reloadMaterials() {
    LOG_DEBUG("Reloading materials...");

    ShaderLoader::clearAllCachedPrograms();
    vg::ShaderManager::disposeAllPrograms();
    for (auto&& entry : mMaterialShaderFiles) {
        mMaterialManager->loadMaterialShader(entry);
    };
    for (auto&& entry : mComputeFiles) {
        mMaterialManager->loadComputeShader(entry);
    };

    LOG_DEBUG("...done");
}

void ResourceManager::generateNormalMaps() {
    //TODO: This not do anything!
    glTextureBarrier();
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
                mBrushFiles.emplace_back(entry);
            }
        }
        else if (fileHasExtension(entry, ".cube")) {
            mCubemapFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".room")) {
            mRoomFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".bldg")) {
            mBuildingFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".tile")) {
            mTileFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".prog")) {
            mMaterialShaderFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".material")) {
            mMaterialFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".comp")) {
            mComputeFiles.emplace_back(entry);
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
        else if (fileHasExtension(entry, ".part")) {
            mParticleSystemFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".ent")) {
            mEntityFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".recipe")) {
            mRecipeFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".item")) {
            mItemFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".fish")) {
            mFishFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".business")) {
            mBusinessFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".model")) {
            mModelFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".rig")) {
            mRigFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".machine")) {
            mAnimMachineFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".skill")) {
            mSkillFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".anim")) {
            mAnimFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".ttf")) {
            mFontFiles.emplace_back(entry);
        }
        else if (fileHasExtension(entry, ".grass")) {
            mTileGrassFiles.emplace_back(entry);
        }
    }
}
