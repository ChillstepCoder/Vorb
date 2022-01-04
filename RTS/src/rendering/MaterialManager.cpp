#include "stdafx.h"
#include "MaterialManager.h"

#include "rendering/SpriteRepository.h"
#include "ShaderLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h> //TODO: Remove
#include <Vorb/graphics/GLProgram.h>

MaterialManager::MaterialManager(vio::IOManager& ioManager, SpriteRepository& spriteRepository, vg::TextureCache& textureCache) :
    mIoManager(ioManager), mSpriteRepository(spriteRepository), mTextureCache(textureCache) {

}

MaterialManager::~MaterialManager() {

}

bool MaterialManager::loadMaterial(const vio::Path& filePath) {
    // Read file
    nString data;
    mIoManager.readFileToString(filePath, data);
    if (data.empty()) return false;

    // Convert to YAML
    keg::ReadContext context;
    context.env = keg::getGlobalEnvironment();
    context.reader.init(data.c_str());
    keg::Node rootObject = context.reader.getFirst();
    if (keg::getType(rootObject) != keg::NodeType::MAP) {
        context.reader.dispose();
        return false;
    }

    auto f = makeFunctor([&](Sender, const nString& key, keg::Node value) {
        MaterialData materialData;
        keg::Error error = keg::parse((ui8*)&materialData, value, context, &KEG_GLOBAL_TYPE(MaterialData));
        assert(error == keg::Error::NONE);

        // Check if material already exists and replace if so
        Material* newMaterial;
        auto&& it = mNameToMaterialIDMap.find(key);
        if (it != mNameToMaterialIDMap.end()) {
            newMaterial = mMaterials[it->second].get();
            newMaterial->dispose();
        }
        else {
            // Store material
            MaterialID newId = (MaterialID)mMaterials.size();
            newMaterial = mMaterials.emplace_back(std::make_unique<Material>()).get();
            mNameToMaterialIDMap[key] = newId;
        }

        // Get shader
        newMaterial->mProgram = ShaderLoader::getOrCreateProgram(materialData.vertexShaderName, materialData.fragmentShaderName, materialData.geometryShaderName, materialData.tessControlShaderName, materialData.tessEvalShaderName);
        assert(newMaterial->mProgram.isLinked());

        for (int i = 0; i < materialData.atlasTextures.size(); ++i) {
            const MaterialAtlasTextureInputData& textureData = materialData.atlasTextures[i];
            const SpriteData& sprite = mSpriteRepository.getSprite(textureData.textureName);
            assert(textureData.uniformPageName.size() && textureData.uniformRectName.size());
            // If this is an atlas subtexture, our material will need the uvrect and page of the texture
            MaterialAtlasTextureInput input;
            input.uvRect = sprite.uvs;
            input.page = sprite.atlasPage;
            input.uvRectUniform = newMaterial->mProgram.getUniform(textureData.uniformRectName.c_str());
            input.pageUniform = newMaterial->mProgram.getUniform(textureData.uniformPageName.c_str());
            newMaterial->mInputAtlasTextures.emplace_back(std::move(input));
        }

        for (int i = 0; i < materialData.textures.size(); ++i) {
            const MaterialTextureInputData& textureData = materialData.textures[i];
            MaterialTextureInput input;
            input.textureUniform = newMaterial->mProgram.getUniform(textureData.uniformName.c_str());
            vg::Texture texture = mTextureCache.findTexture(textureData.textureName);
            input.texture = texture.id;
            assert(texture.id != 0);
            newMaterial->mInputTextures.emplace_back(std::move(input));
        }

        // Get uniforms from shader
        for (auto&& uniform : newMaterial->mProgram.getUniforms()) {
            MaterialUniform matUniform = lookupMaterialUniform(uniform.first);
            if (matUniform != MaterialUniform::INVALID) {
                newMaterial->mUniforms.emplace_back(lookupMaterialUniform(uniform.first), uniform.second);
            }
        }
        
    });
    context.reader.forAllInMap(rootObject, &f);
    context.reader.dispose();

    return true;
}

const Material* MaterialManager::getMaterial(MaterialID id) const {
    return mMaterials.at(id).get();
}

const Material* MaterialManager::getMaterial(const nString& strId) const {
    auto&& it = mNameToMaterialIDMap.find(strId);
    if (it != mNameToMaterialIDMap.end()) {
        return mMaterials[it->second].get();
    }
    std::cout << "Failed to find material " << strId << std::endl;
    assert(false);
    return nullptr;
}
