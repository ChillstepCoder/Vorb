#include "stdafx.h"
#include "ResourceManager.h"

#include "rendering/SpriteRepository.h"
#include "rendering/MaterialManager.h"
#include "rendering/TextureAtlas.h"
#include "rendering/ShaderLoader.h"
#include "particles/ParticleSystemManager.h"
#include "city/Building.h"
#include "ecs/EntityDefinitionRepository.h"
#include "item/ItemRepository.h"
#include "crafting/CraftingRepository.h"
#include "ecs/business/BusinessRepository.h"
#include "character/CharacterModelRepository.h"

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

    mSpriteRepository = std::make_unique<SpriteRepository>(*mIoManager);
    mMaterialManager = std::make_unique<MaterialManager>(*mIoManager, *mSpriteRepository, *mTextureCache);
    mParticleSystemManager = std::make_unique<ParticleSystemManager>(*mIoManager);
    mBuildingRepository = std::make_unique<BuildingDescriptionRepository>(*mIoManager);
    mEntityDefinitionRepository = std::make_unique<EntityDefinitionRepository>(*mIoManager);
    mItemRepository = std::make_unique<ItemRepository>(*mIoManager);
    mCraftingRepository = std::make_unique<CraftingRepository>(*mIoManager);
    mBusinessRepository = std::make_unique<BusinessRepository>(*mIoManager, *mItemRepository);
    mCharacterModelRepository = std::make_unique<CharacterModelRepository>(*mSpriteRepository);
}

ResourceManager::~ResourceManager() {

}

bool fileHasExtension(const vio::Path& filePath, const std::string& extension) {
    size_t length = filePath.getString().size();
    if (filePath.getString().size() < extension.size()) {
        return false;
    }
    return strcmp(filePath.getString().c_str() + (length - extension.size()), extension.c_str()) == 0;
}

void ResourceManager::gatherFiles(const vio::Path& folderPath) {
    gatherRecursive(folderPath);

    mCharacterModelRepository->gatherCharacterModelParts();

    mHasGathered = true;
}

void ResourceManager::loadFiles() {
    assert(mHasGathered);

    // Load Textures
    for (auto&& entry : mTextureFiles) {
        if (vio::containsSubpath(entry, "_noatlas")) {
            // TODO: Allow custom sampler state
            mTextureCache->addTexture(
                entry,
                vio::getLeafNameFromFilePathNoExtension(entry),
                vg::TextureTarget::TEXTURE_2D,
                &vg::SamplerState::LINEAR_WRAP
            );
        }
        else {
            mSpriteRepository->loadSpriteTexture(entry);
        }
    }
    mTextureFiles.clear();

    // Load item definitions
    for (auto&& entry : mItemFiles) {
        mItemRepository->loadItemFile(entry, *mSpriteRepository);
    }

    // Load Tiles
    for (auto&& entry : mTileFiles) {
        // TODO: Tilemanager?
        loadTiles(entry);
    }
    mTileFiles.clear();

    // Load recipe definitions
    for (auto&& entry : mRecipeFiles) {
        mCraftingRepository->loadRecipeFile(*mItemRepository, entry);
    }

    // Load Materials
    for (auto&& entry : mMaterialFiles) {
        mMaterialManager->loadMaterial(entry);
    };
    mMaterialFiles.clear();

    // Load particle Systems
    for (auto&& entry : mParticleSystemFiles) {
        mParticleSystemManager->loadParticleSystemData(entry);
    };
    mMaterialFiles.clear();

    // Load Rooms
    for (auto&& entry : mRoomFiles) {
        mBuildingRepository->loadRoomDescriptionFile(entry);
    }
    mRoomFiles.clear();

    // Load Buildings
    for (auto&& entry : mBuildingFiles) {
        mBuildingRepository->loadBuildingDescriptionFile(entry);
    }
    mBuildingFiles.clear();

    // Update textures
    mSpriteRepository->mTextureAtlas->uploadDirtyPages();

    // Load entity definitions
    for (auto&& entry : mEntityFiles) {
        mEntityDefinitionRepository->loadEntityDefinitionFile(entry);
    }

    // Load business definitions
    for (auto&& entry : mBusinessFiles) {
        mBusinessRepository->loadBusinessFile(entry);
    }

    mHasLoadedResources = true;
}

const SpriteData& ResourceManager::getSprite(const std::string& spriteName) {
    return mSpriteRepository->getSprite(spriteName);
}

vg::TextureCache& ResourceManager::getTextureCache() {
    return *mTextureCache;
}

const TextureAtlas& ResourceManager::getTextureAtlas() const {
    return mSpriteRepository->getTextureAtlas();
}

void ResourceManager::generateNormalMaps() {
    //TODO: This not do anything!
    glTextureBarrier();
}

void ResourceManager::writeDebugAtlas() const {
    mSpriteRepository->mTextureAtlas->writeDebugPages();
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
        // TODO: Map lookup for minor optimization
        if (entry.isDirectory()) {
            gatherRecursive(entry);
        }
        else if (fileHasExtension(entry, ".png")) {
            mTextureFiles.emplace_back(entry);
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
        // TODO: .ttf?
    }
}

bool ResourceManager::loadTiles(const vio::Path& filePath) {
    // Read file
    return mIoManager->parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        TileData tile;

        // Load data
        keg::parse((ui8*)&tile, value, readContext, &KEG_GLOBAL_TYPE(TileData));

        // TODO: Serialize the string > ID mapping
        TileID nextId = ++mIdGenerator;
        assert(nextId < 0xffff); // Make sure we dont roll over
        assert(TileRepository::sTileIdMapping.find(key) == TileRepository::sTileIdMapping.end()); // Duplicate name
        // TODO: error handling  for missing  sprite
        tile.spriteData = getSprite(tile.textureName);
        assert(tile.spriteData.isValid()); // TODO: Error msg
        TileRepository::sTileIdMapping[key] = nextId;
        TileRepository::sTileData[nextId] = std::move(tile);
    }));
}
