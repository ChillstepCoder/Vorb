#include "stdafx.h"
#include "MaterialShaderRepository.h"

#include "resources/TextureRepository.h"
#include "ShaderLoader.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/GLProgram.h>

// TODO: StrToken
const std::map<nString, MaterialShaderUniform> sUniformLookup = {
    std::make_pair("Fbo0", MaterialShaderUniform::Fbo0),
    std::make_pair("FboDepth", MaterialShaderUniform::FboDepth),
    std::make_pair("FboNormals", MaterialShaderUniform::FboNormals),
    std::make_pair("FboRoughness", MaterialShaderUniform::FboRoughness),
    std::make_pair("PrevFbo0", MaterialShaderUniform::PrevFbo0),
    std::make_pair("PrevFboDepth", MaterialShaderUniform::PrevFboDepth),
    std::make_pair("PixelDims", MaterialShaderUniform::PixelDims),
    std::make_pair("SkyRotMatrix", MaterialShaderUniform::SkyRotMatrix),
    std::make_pair("ScreenResolution", MaterialShaderUniform::ScreenResolution),
    std::make_pair("ShadowColor", MaterialShaderUniform::ShadowColor),
    std::make_pair("SSAOTexture", MaterialShaderUniform::SSAOTexture),
    std::make_pair("SSAOColor", MaterialShaderUniform::SSAOColor),
    std::make_pair("DebugColor1", MaterialShaderUniform::DebugColor1),
    std::make_pair("DebugColor2", MaterialShaderUniform::DebugColor2),
    std::make_pair("DebugFloat1", MaterialShaderUniform::DebugFloat1),
    std::make_pair("DebugFloat2", MaterialShaderUniform::DebugFloat2),
    std::make_pair("DebugFloat3", MaterialShaderUniform::DebugFloat3),
    std::make_pair("DebugFloat4", MaterialShaderUniform::DebugFloat4),
};
static_assert((int)MaterialShaderUniform::COUNT == 19, "Update for new material uniform");

MaterialShaderUniform lookupMaterialUniform(const nString& str) {
    // For arrays we remove the array syntax
    if (str[str.size() - 1] == ']') {
        auto&& it = sUniformLookup.find(str.substr(0, str.size() - 3));
        if (it != sUniformLookup.end()) {
            return it->second;
        }
    }
    else {
        auto&& it = sUniformLookup.find(str);
        if (it != sUniformLookup.end()) {
            return it->second;
        }
    }
    return MaterialShaderUniform::INVALID;
}

struct MaterialShaderFileData {
    std::vector<MaterialTextureInputData> textures;
    nString vertexShaderName;
    nString fragmentShaderName;
    nString geometryShaderName;
    nString tessControlShaderName;
    nString tessEvalShaderName;
};
SERIALIZABLE_SIMPLE(MaterialShaderFileData,
    make_field(o.textures, "textures"sv),
    make_field(o.vertexShaderName, "vert"sv),
    make_field(o.fragmentShaderName, "frag"sv),
    make_field(o.geometryShaderName, "geom"sv),
    make_field(o.tessControlShaderName, "tcs"sv),
    make_field(o.tessEvalShaderName, "tes"sv)
);

AssetLoadFunc MaterialShaderRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        MaterialShaderFileData& fileData = std::any_cast<MaterialShaderFileData&>(userData);
        MaterialShaderDef& def = *static_cast<MaterialShaderDef*>(assetDataPtr);

        // Clear in case of reload
        def.mInputTextures.clear();
        def.mUniforms.clear();

        // TODO: Can we do any work here? If not, can we have it send directly to the render thread?
        if (filePath.getExtension() == "comp") {
            def.mIsCompute = true;
        }
        else {
            YmlSerializer::readFileData(readFileToString(filePath), fileData);
            def.mIsCompute = false;
            // Texture dependencies
            for (int i = 0; i < fileData.textures.size(); ++i) {
                const MaterialTextureInputData& textureData = fileData.textures[i];
                def.addDependency(TextureRepository::get().getAssetHandle(textureData.textureName));
            }
        }

        assetLoader.requestAssetLoadWithDependencies(
            nullptr,
            [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
            MaterialShaderFileData& fileData = std::any_cast<MaterialShaderFileData&>(userData);
            MaterialShaderDef& def = *static_cast<MaterialShaderDef*>(assetDataPtr);
            if (def.mIsCompute) {
                // Compute shader
                def.mProgram = ShaderLoader::createComputeProgramFromFile(def.getName().toString(), filePath);
            }
            else {

                def.mProgram = ShaderLoader::getOrCreateProgram(fileData.vertexShaderName, fileData.fragmentShaderName, fileData.geometryShaderName, fileData.tessControlShaderName, fileData.tessEvalShaderName);
                assert(def.mProgram.isLinked());


                for (int i = 0; i < fileData.textures.size(); ++i) {
                    const MaterialTextureInputData& textureData = fileData.textures[i];
                    MaterialTextureInput& input = def.mInputTextures.emplace_back();
                    input.textureUniform = def.mProgram.getUniform(textureData.uniformName.c_str());
                    input.texture = TextureRepository::get().getLoadedOrUnloadedAsset(textureData.textureName).gpuTexture.getHandle();
                    assert(input.texture != 0);
                }

                // Get uniforms from shader
                for (auto&& uniform : def.mProgram.getUniforms()) {
                    MaterialShaderUniform matUniform = lookupMaterialUniform(uniform.first);
                    if (matUniform != MaterialShaderUniform::INVALID) {
                        def.mUniforms.emplace_back(lookupMaterialUniform(uniform.first), uniform.second);
                    }
                }

            }
            return true;
        },
            assetID,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetID].get(),
            std::move(fileData),
            def.getDependencies()
        );

        return false;
    };
}

std::any MaterialShaderRepository::getUserData(AssetID assetId) {
    return MaterialShaderFileData();
}
