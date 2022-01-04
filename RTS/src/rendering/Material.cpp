#include "stdafx.h"
#include "Material.h"

#include <Vorb/graphics/Texture.h>


KEG_TYPE_DEF_SAME_NAME(MaterialAtlasTextureInputData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, textureName), keg::BasicType::STRING));
    kt.addValue("unrect", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, uniformRectName), keg::BasicType::STRING));
    kt.addValue("unpage", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, uniformPageName), keg::BasicType::STRING));
}

KEG_TYPE_DEF_SAME_NAME(MaterialTextureInputData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(MaterialTextureInputData, textureName), keg::BasicType::STRING));
    kt.addValue("uniform", keg::Value::basic(offsetof(MaterialTextureInputData, uniformName), keg::BasicType::STRING));
}

KEG_TYPE_DEF_SAME_NAME(MaterialData, kt) {
    kt.addValue("atlas_textures", keg::Value::array(offsetof(MaterialData, atlasTextures), keg::Value::custom(0, "MaterialAtlasTextureInputData", false)));
    kt.addValue("textures", keg::Value::array(offsetof(MaterialData, textures), keg::Value::custom(0, "MaterialTextureInputData", false)));
    kt.addValue("vert", keg::Value::basic(offsetof(MaterialData, vertexShaderName), keg::BasicType::STRING));
    kt.addValue("frag", keg::Value::basic(offsetof(MaterialData, fragmentShaderName), keg::BasicType::STRING));
    kt.addValue("geom", keg::Value::basic(offsetof(MaterialData, geometryShaderName), keg::BasicType::STRING));
    kt.addValue("tcs", keg::Value::basic(offsetof(MaterialData, tessControlShaderName), keg::BasicType::STRING));
    kt.addValue("tes", keg::Value::basic(offsetof(MaterialData, tessEvalShaderName), keg::BasicType::STRING));
}

const std::map<nString, MaterialUniform> sUniformLookup = {
    std::make_pair("Atlas", MaterialUniform::Atlas),
    std::make_pair("Fbo0", MaterialUniform::Fbo0),
    std::make_pair("FboLight", MaterialUniform::FboLight),
    std::make_pair("FboDepth", MaterialUniform::FboDepth),
    std::make_pair("FboNormals", MaterialUniform::FboNormals),
    std::make_pair("FboRoughness", MaterialUniform::FboRoughness),
    std::make_pair("PrevFbo0", MaterialUniform::PrevFbo0),
    std::make_pair("PrevFboDepth", MaterialUniform::PrevFboDepth),
    std::make_pair("PixelDims", MaterialUniform::PixelDims),
    std::make_pair("ZoomScale", MaterialUniform::ZoomScale),
    std::make_pair("FboZCutout", MaterialUniform::FboZCutout),
    std::make_pair("CameraZAngle", MaterialUniform::CameraZAngle),
    std::make_pair("SkyRotMatrix", MaterialUniform::SkyRotMatrix),
    std::make_pair("ScreenResolution", MaterialUniform::ScreenResolution),
    std::make_pair("ShadowFrustumMatrices", MaterialUniform::ShadowFrustumMatrices),
    std::make_pair("ShadowMap", MaterialUniform::ShadowMap),
    std::make_pair("ShadowCascadePlaneDistances", MaterialUniform::ShadowCascadePlaneDistances),
    std::make_pair("ShadowColor", MaterialUniform::ShadowColor),
    std::make_pair("ShadowTexture", MaterialUniform::ShadowTexture),
    std::make_pair("SSAOTexture", MaterialUniform::SSAOTexture),
    std::make_pair("SSAOColor", MaterialUniform::SSAOColor),
};
static_assert((int)MaterialUniform::COUNT == 22, "Update for new material uniform");

extern MaterialUniform lookupMaterialUniform(const nString& str) {
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
    return MaterialUniform::INVALID;
}

void Material::use(OUT ui32& nextAvailableTextureIndex) const {

    mProgram.use();

    for (auto&& atlasTextureInput : mInputAtlasTextures) {
        glUniform4fv(atlasTextureInput.uvRectUniform, 1, &atlasTextureInput.uvRect[0]);
        glUniform1fv(atlasTextureInput.pageUniform, 1, &atlasTextureInput.page);
    }

    for (auto&& textureInput : mInputTextures) {
        glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
        glUniform1i(textureInput.textureUniform, nextAvailableTextureIndex++);
        glBindTexture(GL_TEXTURE_2D, textureInput.texture);
    }
}

void Material::dispose() {
    mUniforms.clear();
    mInputAtlasTextures.clear();
    mInputTextures.clear();
}
