#include "stdafx.h"
#include "MaterialManager.h"

#include "resources/TextureRepository.h"
#include "ShaderLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h> //TODO: Remove
#include <Vorb/graphics/GLProgram.h>

MaterialManager::MaterialManager(vio::IOManager& ioManager, TextureRepository& textureRepository, vg::TextureCache& textureCache) :
    mIoManager(ioManager), mTextureRepository(textureRepository), mTextureCache(textureCache) {

}

MaterialManager::~MaterialManager() {

}

bool MaterialManager::loadMaterialShader(const vio::Path& filePath) {
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
        MaterialShaderData materialData;
        keg::Error error = keg::parse((ui8*)&materialData, value, context, &KEG_GLOBAL_TYPE(MaterialShaderData));
        assert(error == keg::Error::NONE);

        // Check if material already exists and replace if so
        MaterialShader* newMaterial;
        auto&& it = mNameToMaterialIDMap.find(key);
        if (it != mNameToMaterialIDMap.end()) {
            newMaterial = mMaterials[it->second].get();
            newMaterial->dispose();
        }
        else {
            // Store material
            MaterialID newId = (MaterialID)mMaterials.size();
            newMaterial = mMaterials.emplace_back(std::make_unique<MaterialShader>()).get();
            mNameToMaterialIDMap[key] = newId;
        }

        // Get shader
        newMaterial->mProgram = ShaderLoader::getOrCreateProgram(materialData.vertexShaderName, materialData.fragmentShaderName, materialData.geometryShaderName, materialData.tessControlShaderName, materialData.tessEvalShaderName);
        assert(newMaterial->mProgram.isLinked());

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
            MaterialShaderUniform matUniform = lookupMaterialUniform(uniform.first);
            if (matUniform != MaterialShaderUniform::INVALID) {
                newMaterial->mUniforms.emplace_back(lookupMaterialUniform(uniform.first), uniform.second);
            }
        }
        
    });
    context.reader.forAllInMap(rootObject, &f);
    context.reader.dispose();

    return true;
}

bool MaterialManager::loadComputeShader(const vio::Path& filePath) {
    nString name = filePath.getFileNameNoExtension();
    vg::GLProgram newProgram = ShaderLoader::createComputeProgramFromFile(name, filePath);
    mComputeShaders[name] = std::move(newProgram);
    return true;
}

const MaterialShader* MaterialManager::getMaterial(MaterialID id) const {
    return mMaterials.at(id).get();
}

const MaterialShader* MaterialManager::getMaterial(const nString& strId) const {
    auto&& it = mNameToMaterialIDMap.find(strId);
    if (it != mNameToMaterialIDMap.end()) {
        return mMaterials[it->second].get();
    }
    LOG_CRITICAL("Failed to find material {}", strId);
    assert(false);
    return nullptr;
}

const vg::GLProgram* MaterialManager::getComputeShader(const nString& strId) const {
    auto&& it = mComputeShaders.find(strId);
    if (it != mComputeShaders.end()) {
        return &it->second;
    }
    LOG_CRITICAL("Failed to find compute shader {}", strId);
    assert(false);
    return nullptr;
}
