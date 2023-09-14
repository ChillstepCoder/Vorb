#include "stdafx.h"
#include "MaterialShaderManager.h"

#include "resources/TextureRepository.h"
#include "ShaderLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/GLProgram.h>

MaterialShaderManager::MaterialShaderManager(vio::IOManager& ioManager) :
    mIoManager(ioManager) {

}

MaterialShaderManager::~MaterialShaderManager() {

}

bool MaterialShaderManager::loadMaterialShader(const vio::Path& filePath) {
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
        auto&& it = mNameToMaterialShaderMap.find(key);
        if (it != mNameToMaterialShaderMap.end()) {
            newMaterial = mMaterialShaders[it->second].get();
            newMaterial->dispose();
        }
        else {
            // Store material
            ui32 newId = (ui32)mMaterialShaders.size();
            newMaterial = mMaterialShaders.emplace_back(std::make_unique<MaterialShader>()).get();
            mNameToMaterialShaderMap[key] = newId;
        }

        // Get shader
        newMaterial->mProgram = ShaderLoader::getOrCreateProgram(materialData.vertexShaderName, materialData.fragmentShaderName, materialData.geometryShaderName, materialData.tessControlShaderName, materialData.tessEvalShaderName);
        assert(newMaterial->mProgram.isLinked());

        for (int i = 0; i < materialData.textures.size(); ++i) {
            const MaterialTextureInputData& textureData = materialData.textures[i];
            MaterialTextureInput input;
            input.textureUniform = newMaterial->mProgram.getUniform(textureData.uniformName.c_str());
            input.texture = mTextureRepository.getTexture(textureData.textureName).texture.getHandle();
            assert(input.texture != 0);
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

bool MaterialShaderManager::loadComputeShader(const vio::Path& filePath) {
    nString name = filePath.getFileNameNoExtension();
    vg::GLProgram newProgram = ShaderLoader::createComputeProgramFromFile(name, filePath);
    mComputeShaders[name] = std::move(newProgram);
    return true;
}

const MaterialShader* MaterialShaderManager::getMaterialShader(const nString& strId) const {
    auto&& it = mNameToMaterialShaderMap.find(strId);
    if (it != mNameToMaterialShaderMap.end()) {
        return mMaterialShaders[it->second].get();
    }
    LOG_CRITICAL("Failed to find shader {}", strId);
    assert(false);
    return nullptr;
}

const vg::GLProgram* MaterialShaderManager::getComputeShader(const nString& strId) const {
    auto&& it = mComputeShaders.find(strId);
    if (it != mComputeShaders.end()) {
        return &it->second;
    }
    LOG_CRITICAL("Failed to find compute shader {}", strId);
    assert(false);
    return nullptr;
}
