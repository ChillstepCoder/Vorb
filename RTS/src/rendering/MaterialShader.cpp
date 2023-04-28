#include "stdafx.h"
#include "MaterialShader.h"

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

KEG_TYPE_DEF_SAME_NAME(MaterialShaderData, kt) {
    kt.addValue("textures", keg::Value::array(offsetof(MaterialShaderData, textures), keg::Value::custom(0, "MaterialTextureInputData", false)));
    kt.addValue("vert", keg::Value::basic(offsetof(MaterialShaderData, vertexShaderName), keg::BasicType::STRING));
    kt.addValue("frag", keg::Value::basic(offsetof(MaterialShaderData, fragmentShaderName), keg::BasicType::STRING));
    kt.addValue("geom", keg::Value::basic(offsetof(MaterialShaderData, geometryShaderName), keg::BasicType::STRING));
    kt.addValue("tcs", keg::Value::basic(offsetof(MaterialShaderData, tessControlShaderName), keg::BasicType::STRING));
    kt.addValue("tes", keg::Value::basic(offsetof(MaterialShaderData, tessEvalShaderName), keg::BasicType::STRING));
}

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

extern MaterialShaderUniform lookupMaterialUniform(const nString& str) {
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

void MaterialShader::use(OUT ui32& nextAvailableTextureIndex) const {

    mProgram.use();

    for (auto&& textureInput : mInputTextures) {
        glBindTextureUnit(nextAvailableTextureIndex, textureInput.texture);
        glUniform1i(textureInput.textureUniform, nextAvailableTextureIndex++);
    }
}

void MaterialShader::dispose() {
    mUniforms.clear();
    mInputTextures.clear();
}
