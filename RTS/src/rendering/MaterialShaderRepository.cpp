#include "stdafx.h"
#include "MaterialShaderRepository.h"

#include "resources/TextureRepository.h"
#include "ShaderLoader.h"

#include <Vorb/graphics/GLProgram.h>

SERIALIZABLE_SIMPLE(ShaderDefine,
    make_field(o.name, "name"sv),
    make_field(o.active, "active"sv)
);

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
    ShaderDefinesVector defines;
};
SERIALIZABLE_SIMPLE(MaterialShaderFileData,
    make_field(o.textures, "textures"sv),
    make_field(o.vertexShaderName, "vert"sv),
    make_field(o.fragmentShaderName, "frag"sv),
    make_field(o.geometryShaderName, "geom"sv),
    make_field(o.tessControlShaderName, "tcs"sv),
    make_field(o.tessEvalShaderName, "tes"sv),
    make_field(o.defines, "defines"sv)
);

bool MaterialShaderRepository::assetSourceIsDirty(AssetID assetId) {
    bool dirty = IAssetRepository::assetSourceIsDirty(assetId);
    MaterialShaderDef& def = *mAssets[assetId];

    if (!def.mIsCompute) {
        if (!def.mShaderNames) {
            dirty = true;
        }
        else {
            if (def.mShaderNames->vertex.size()) {
                dirty |= ShaderLoader::refreshFileWriteTime(def.mShaderNames->vertex, ShaderType::Vertex, def.mShaderNames->vertexWriteTime);
            }
            if (def.mShaderNames->fragment.size()) {
                dirty |= ShaderLoader::refreshFileWriteTime(def.mShaderNames->fragment, ShaderType::Fragment, def.mShaderNames->fragmentWriteTime);
            }
            if (def.mShaderNames->geometry.size()) {
                dirty |= ShaderLoader::refreshFileWriteTime(def.mShaderNames->geometry, ShaderType::Geometry, def.mShaderNames->geometryWriteTime);
            }
            if (def.mShaderNames->tessControl.size()) {
                dirty |= ShaderLoader::refreshFileWriteTime(def.mShaderNames->tessControl, ShaderType::TessControl, def.mShaderNames->tessControlWriteTime);
            }
            if (def.mShaderNames->tessEval.size()) {
                dirty |= ShaderLoader::refreshFileWriteTime(def.mShaderNames->tessEval, ShaderType::TessEval, def.mShaderNames->tessEvalWriteTime);
            }
        }
    }

    return dirty;
}

void MaterialShaderRepository::preReloadAsset(AssetID assetId) {
    if (mLoadedAssets[assetId]) {
        MaterialShaderDef& def = *mAssets[assetId];
        if (def.mProgram.isLinked()) {
            def.mProgram.dispose();
        }
        def.mInputTextures.clear();
        def.mUniforms.clear();
        def.mDefines.clear();
    }
}

AssetLoadFunc MaterialShaderRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        MaterialShaderFileData& fileData = std::any_cast<MaterialShaderFileData&>(userData);
        MaterialShaderDef& def = *static_cast<MaterialShaderDef*>(assetDataPtr);

        // TODO: Can we do any work here? If not, can we have it send directly to the render thread?
        if (filePath.getExtension() == "comp") {
            def.mIsCompute = true;
        }
        else {
            YmlSerializer::readFileData(readFileToString(filePath), fileData);

            def.mDefines = std::move(fileData.defines);
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
                const fs::path stdPath = filePath.getStdPath();

                def.mProgram = ShaderLoader::createProgram(
                    def.getName().toString(),
                    fileData.vertexShaderName,
                    fileData.fragmentShaderName,
                    fileData.geometryShaderName,
                    fileData.tessControlShaderName,
                    fileData.tessEvalShaderName,
                    &def.mDefines
                );

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
            def.mShaderNames = std::make_unique<MaterialShaderNames>();
            def.mShaderNames->vertex = std::move(fileData.vertexShaderName);
            def.mShaderNames->fragment = std::move(fileData.fragmentShaderName);
            def.mShaderNames->geometry = std::move(fileData.geometryShaderName);
            def.mShaderNames->tessControl = std::move(fileData.tessControlShaderName);
            def.mShaderNames->tessEval = std::move(fileData.tessEvalShaderName);
            ShaderLoader::refreshFileWriteTime(def.mShaderNames->vertex, ShaderType::Vertex, def.mShaderNames->vertexWriteTime);
            ShaderLoader::refreshFileWriteTime(def.mShaderNames->fragment, ShaderType::Fragment, def.mShaderNames->fragmentWriteTime);
            ShaderLoader::refreshFileWriteTime(def.mShaderNames->geometry, ShaderType::Geometry, def.mShaderNames->geometryWriteTime);
            ShaderLoader::refreshFileWriteTime(def.mShaderNames->tessControl, ShaderType::TessControl, def.mShaderNames->tessControlWriteTime);
            ShaderLoader::refreshFileWriteTime(def.mShaderNames->tessEval, ShaderType::TessEval, def.mShaderNames->tessEvalWriteTime);

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
