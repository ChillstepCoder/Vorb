#include "stdafx.h"
#include "ResourceManager.h"

#include "rendering/MaterialManager.h"
#include "rendering/TextureAtlas.h"
#include "rendering/ShaderLoader.h"
#include "particles/ParticleSystemManager.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/IO.h>
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

    mSpriteRepository = std::make_unique<SpriteRepository>(*mIoManager);
    mMaterialManager = std::make_unique<MaterialManager>(*mIoManager, *mSpriteRepository);
    mParticleSystemManager = std::make_unique<ParticleSystemManager>(*mIoManager);
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
        if (entry.isDirectory()) {
            gatherFiles(entry);
        } else if (fileHasExtension(entry, ".png")) {
            mTextureFiles.emplace_back(entry);
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
        // TODO: .ttf?
    }

    mHasGathered = true;
}

void ResourceManager::loadFiles() {
    assert(mHasGathered);

    // Load Textures
    for (auto&& entry : mTextureFiles) {
        mSpriteRepository->loadSpriteTexture(entry);
    }
    mTextureFiles.clear();

    // Load Tiles
    for (auto&& entry : mTileFiles) {
        // TODO: Tilemanager?
        loadTiles(entry);
    }
    mTileFiles.clear();

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

    // Update textures
    mSpriteRepository->mTextureAtlas->uploadDirtyPages();

    mHasLoadedResources = true;
}

const SpriteData& ResourceManager::getSprite(const std::string& spriteName) {
    return mSpriteRepository->getSprite(spriteName);
}

void ResourceManager::generateNormalMaps() {

    glTextureBarrier();
}

void ResourceManager::writeDebugAtlas() const {
    mSpriteRepository->mTextureAtlas->writeDebugPages();
}

bool ResourceManager::loadTiles(const vio::Path& filePath) {
    // Read file
    nString data;
    mIoManager->readFileToString(filePath, data);
    if (data.empty()) return false;

    // Convert to YAML
    keg::ReadContext context;
    context.env = keg::getGlobalEnvironment();
    context.reader.init(data.c_str());
    keg::Node node = context.reader.getFirst();
    if (keg::getType(node) != keg::NodeType::MAP) {
        context.reader.dispose();
        return false;
    }

    auto f = makeFunctor([&](Sender, const nString& key, keg::Node value) {
        TileData tile;

        // Load data
        keg::parse((ui8*)&tile, value, context, &KEG_GLOBAL_TYPE(TileData));

        // TODO: Serialize the string > ID mapping
        TileID nextId = ++mIdGenerator;
        assert(nextId < 0xffff); // Make sure we dont roll over
        assert(TileRepository::sTileIdMapping.find(key) == TileRepository::sTileIdMapping.end()); // Duplicate name
        // TODO: error handling  for missing  sprite
        tile.spriteData = getSprite(tile.textureName);
        assert(tile.spriteData.isValid()); // TODO: Error msg
        TileRepository::sTileIdMapping[key] = nextId;
        TileRepository::sTileData[nextId] = std::move(tile);
    });
    context.reader.forAllInMap(node, &f);
    context.reader.dispose();

    return true;
}
