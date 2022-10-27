#include "stdafx.h"
#include "resources/ResourceManager.h"

#include "rendering/MaterialManager.h"
#include "rendering/ShaderLoader.h"
#include "particles/ParticleSystemManager.h"
#include "city/Building.h"
#include "city/BuildingDescriptionRepository.h"
#include "ecs/EntityDefinitionRepository.h"
#include "item/ItemRepository.h"
#include "crafting/CraftingRepository.h"
#include "ecs/business/BusinessRepository.h"
#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "resources/SkillRepository.h"
#include "resources/TextureRepository.h"
#include "resources/TileRepository.h"
#include "resources/FontRepository.h"
#include "editor/BrushRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/IO.h>
#include <vorb/io/FileOps.h>
#include <Vorb/graphics/TextureCache.h>
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
    mTextureCache = std::make_unique<vg::TextureCache>();
    mTextureCache->init(mIoManager.get());

    mTextureRepository = std::make_unique<TextureRepository>(*mTextureCache, *mIoManager);
    mMaterialManager = std::make_unique<MaterialManager>(*mIoManager, *mTextureRepository, *mTextureCache);
    mParticleSystemManager = std::make_unique<ParticleSystemManager>(*mIoManager);
    mBuildingRepository = std::make_unique<BuildingDescriptionRepository>(*mIoManager);
    mEntityDefinitionRepository = std::make_unique<EntityDefinitionRepository>(*mIoManager);
    mItemRepository = std::make_unique<ItemRepository>(*mIoManager);
    mCraftingRepository = std::make_unique<CraftingRepository>(*mIoManager);
    mBusinessRepository = std::make_unique<BusinessRepository>(*mIoManager, *mItemRepository);
    mAnimationRepository = std::make_unique<AnimationRepository>();
    mRigRepository = std::make_unique<RigRepository>(*mIoManager);
    mAnimMachineRepository = std::make_unique<AnimMachineRepository>(*mIoManager, *mRigRepository);
    mModelRepository = std::make_unique<ModelRepository>(*mIoManager, *mTextureCache, *mRigRepository);
    mBrushRepository = std::make_unique<BrushRepository>(*mIoManager);
    mSkillRepository = std::make_unique<SkillRepository>(*mIoManager);
    mFontRepository = std::make_unique<FontRepository>();
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

void ResourceManager::gatherFiles(const vio::Path& folderPath) {

    PreciseTimer timer;

    // Make sure we clear all vectors each gather
    mTextureFiles.clear();
    mMaterialFiles.clear();
    mTileFiles.clear();
    mParticleSystemFiles.clear();
    mRoomFiles.clear();
    mBuildingFiles.clear();
    mEntityFiles.clear();
    mItemFiles.clear();
    mRecipeFiles.clear();
    mBusinessFiles.clear();
    mModelFiles.clear();
    mAnimFiles.clear();
    mRigFiles.clear();
    mAnimMachineFiles.clear();
    mSkillFiles.clear();
    mFontFiles.clear();

    gatherRecursive(folderPath);

    mHasGathered = true;

    LOG_TRACE("Gathered files in {:.4} ms", timer.stop());
}

void ResourceManager::loadFiles() {
    assert(mHasGathered);

    PreciseTimer totalTimer;

    {
        ScopedTimer timer("NEW: Texture load");
        for (auto&& entry : mTextureFiles) {
            if (vio::containsSubpath(entry, "_brushes")) {
                mBrushRepository->loadBrush(entry, *mTextureCache);
            }
            else if (!vio::containsSubpath(entry, "_loadscreen")) { // Ignore loadscreen files as we manually load them
                mTextureRepository->loadTexture(entry);
            }
        }
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

    // Load Tiles (Must be done after item and recipes)
    // Assuming single tile per file, definitely less than actual but, good enough. 10 is arbitrary
    {
        ScopedTimer timer("Tile load");
        TileRepository::sTileData.reserve(mTileFiles.size() + 10);
        for (auto&& entry : mTileFiles) {
            // TODO: Tilemanager?
            loadTiles(entry);
        }
    }

    // Load Materials
    {
        ScopedTimer timer("Material load");
        for (auto&& entry : mMaterialFiles) {
            mMaterialManager->loadMaterial(entry);
        };
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
            mModelRepository->loadModelFile(entry, *mAnimMachineRepository);
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
        for (auto&& entry : mParticleSystemFiles) {
            mParticleSystemManager->loadParticleSystemData(entry);
        };
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
    LOG_TRACE("Loaded resources in {:.4} ms");
}

const SubTexture& ResourceManager::getTexture(const nString& textureName) const {
    return mTextureRepository->getTexture(textureName);
}

vg::TextureCache& ResourceManager::getTextureCache() {
    return *mTextureCache;
}

void ResourceManager::reloadMaterials() {
    LOG_DEBUG("Reloading materials...");

    ShaderLoader::clearAllCachedPrograms();
    vg::ShaderManager::disposeAllPrograms();
    for (auto&& entry : mMaterialFiles) {
        mMaterialManager->loadMaterial(entry);
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
        // TODO: Better error messaging
        assert(false);
    }

    vio::DirectoryEntries entries;
    if (!directory.appendEntries(entries)) {
        // Empty directory
        return;
    }

    for (auto&& entry : entries) {
        // Recurse
        // TODO: Map lookup for minor optimization? Sort by extension? idk, (im starting to hate this)
        if (entry.isDirectory()) {
            gatherRecursive(entry);
        }
        else if (fileHasExtension(entry, ".png")) {
            // Ignore .norm files they will be grabbed
            // automatically if needed
            const nString& str = entry.getString();
            if (str.size() <= sizeof(".norm.png") || (strcmp(&str[str.size() - 9], ".norm.png") != 0)) {
                mTextureFiles.emplace_back(entry);
            }
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
        else if (fileHasExtension(entry, ".material")) {
            mMaterialFiles.emplace_back(entry);
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
    }
}

bool ResourceManager::loadTiles(const vio::Path& filePath) {
    // TODO: Non arbitrary?
    // Read file
    return mIoManager->parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        TileData tileData;
        TileFileData fileData;

        // Load data
        keg::parse((ui8*)&fileData, value, readContext, &KEG_GLOBAL_TYPE(TileFileData));
        tileData.name = key;

        // Copy all data
        tileData.layer = fileData.layer;
        tileData.pathWeight = fileData.pathWeight;
        tileData.resource = fileData.resource;
        tileData.shape = fileData.tileShape;
        tileData.textureMethod = fileData.textureMethod;
        tileData.dims = fileData.dims;
        // Collider
        //tileData.collider.shape = fileData.colliderShape;
        //if (fileData.colliderShape != TileCollisionShape::NONE) {
        //    // TODO: Doors and shit? Move?
        //    tileData.collider.defaultFlags = (TileFlags)0;
        //    tileData.collider.dims = fileData.colliderDims;
        //}

        // Item drops
        tileData.itemDrops.resize(fileData.itemDrops.size());
        for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
            tileData.itemDrops[i].countRange = fileData.itemDrops[i].countRange;
            tileData.itemDrops[i].id = mItemRepository->getItem(fileData.itemDrops[i].itemName).getID();
        }

        // Recipes
        tileData.recipe.resize(fileData.recipe.size());
        for (size_t i = 0; i < tileData.recipe.size(); ++i) {
            tileData.recipe[i].quantity = fileData.recipe[i].count;
            tileData.recipe[i].id = mItemRepository->getItem(fileData.recipe[i].itemName).getID();
        }

        // Nav bits
        if (tileData.shape == TileShape::STAIRS) {
            tileData.navMask = 0b01000010; // SOUTH and NORTH access
            tileData.heightOffsetSouth = STAIR_TILE_HEIGHT + 0.1f;
            tileData.heightOffsetNorth = 0.0f;
            tileData.heightOffsetWest = 0.0f;
            tileData.heightOffsetEast = 0.0f;
        }
        else {
            tileData.heightOffsetSouth = 0.0f;
            tileData.heightOffsetWest = 0.0f;
            tileData.heightOffsetEast = 0.0f;
            tileData.heightOffsetNorth = 0.0f;
        }

        TileID nextId = (TileID)TileRepository::sTileData.size();
        tileData.id = nextId;
        assert(nextId < UINT16_MAX); // Make sure we dont roll over
        assert(TileRepository::sTileIdMapping.find(key) == TileRepository::sTileIdMapping.end()); // Duplicate name
        // TODO: error handling  for missing  sprite
        tileData.texture = getTexture(fileData.textureName);
        TileRepository::sTileIdMapping[key] = nextId;
        // TODO: Serialize the string > ID mapping
        TileRepository::sTileData.emplace_back(std::move(tileData));
    }));
}
