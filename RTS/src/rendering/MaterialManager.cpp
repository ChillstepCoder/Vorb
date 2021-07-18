#include "stdafx.h"
#include "MaterialManager.h"

#include "rendering/SpriteRepository.h"
#include "ShaderLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h> //TODO: Remove
#include <Vorb/graphics/GLProgram.h>

MaterialManager::MaterialManager(vio::IOManager& ioManager, SpriteRepository& spriteRepository) :
    mIoManager(ioManager), mSpriteRepository(spriteRepository) {

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

        Material newMaterial;

        // Get shader
        newMaterial.mProgram = ShaderLoader::getOrCreateProgram(materialData.vertexShaderName, materialData.fragmentShaderName);
        assert(newMaterial.mProgram.isLinked());

        for (int i = 0; i < materialData.atlasTextures.size(); ++i) {
            const MaterialAtlasTextureInputData& textureData = materialData.atlasTextures[i];
            const SpriteData& sprite = mSpriteRepository.getSprite(textureData.textureName);
            assert(textureData.uniformPageName.size() && textureData.uniformRectName.size());
            // If this is an atlas subtexture, our material will need the uvrect and page of the texture
            MaterialAtlasTextureInput input;
            input.uvRect = sprite.uvs;
            input.page = sprite.atlasPage;
            input.uvRectUniform = newMaterial.mProgram.getUniform(textureData.uniformRectName);
            input.pageUniform = newMaterial.mProgram.getUniform(textureData.uniformPageName);
            newMaterial.mInputAtlasTextures.emplace_back(std::move(input));
        }

        // Get uniforms from shader
        for (auto&& uniform : newMaterial.mProgram.getUniforms()) {
            MaterialUniform matUniform = lookupMaterialUniform(uniform.first);
            if (matUniform != MaterialUniform::INVALID) {
                newMaterial.mUniforms.emplace_back(lookupMaterialUniform(uniform.first), uniform.second);
            }
        }

        // Store material
        MaterialID newId = (MaterialID)mMaterials.size();
        mMaterials.emplace_back(std::move(newMaterial));
        mNameToMaterialIDMap[key] = newId;
        
    });
    context.reader.forAllInMap(rootObject, &f);
    context.reader.dispose();

    return true;
}

const Material* MaterialManager::getMaterial(MaterialID id) const {
    return &mMaterials.at(id);
}

const Material* MaterialManager::getMaterial(const nString& strId) const {
    auto&& it = mNameToMaterialIDMap.find(strId);
    if (it != mNameToMaterialIDMap.end()) {
        return getMaterial(it->second);
    }
    return nullptr;
}
