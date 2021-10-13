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
}

const std::map<nString, MaterialUniform> sUniformLookup = {
    std::make_pair("Time", MaterialUniform::Time),
    std::make_pair("TimeOfDay", MaterialUniform::TimeOfDay),
    std::make_pair("SunColor", MaterialUniform::SunColor),
    std::make_pair("SunHeight", MaterialUniform::SunHeight),
    std::make_pair("SunPosition", MaterialUniform::SunPosition),
    std::make_pair("V", MaterialUniform::VMatrix),
    std::make_pair("InverseV", MaterialUniform::InverseVMatrix),
    std::make_pair("P", MaterialUniform::PMatrix),
    std::make_pair("InverseP", MaterialUniform::InversePMatrix),
    std::make_pair("VP", MaterialUniform::VPMatrix),
    std::make_pair("InverseVP", MaterialUniform::InverseVPMatrix),
    std::make_pair("Atlas", MaterialUniform::Atlas),
    std::make_pair("Fbo0", MaterialUniform::Fbo0),
    std::make_pair("FboLight", MaterialUniform::FboLight),
    std::make_pair("FboDepth", MaterialUniform::FboDepth),
    std::make_pair("FboNormals", MaterialUniform::FboNormals),
    std::make_pair("PrevFbo0", MaterialUniform::PrevFbo0),
    std::make_pair("PrevFboDepth", MaterialUniform::PrevFboDepth),
    std::make_pair("PixelDims", MaterialUniform::PixelDims),
    std::make_pair("ZoomScale", MaterialUniform::ZoomScale),
    std::make_pair("FboZCutout", MaterialUniform::FboZCutout),
    std::make_pair("PlayerPosWorld", MaterialUniform::PlayerPosWorld),
    std::make_pair("CameraRight", MaterialUniform::CameraRight),
    std::make_pair("CameraFront", MaterialUniform::CameraFront),
    std::make_pair("CameraPos", MaterialUniform::CameraPos),
    std::make_pair("CameraZAngle", MaterialUniform::CameraZAngle),
    std::make_pair("SkyRotMatrix", MaterialUniform::SkyRotMatrix),
};
static_assert((int)MaterialUniform::COUNT == 28, "Update for new material uniform");

extern MaterialUniform lookupMaterialUniform(const nString& str) {
    auto&& it = sUniformLookup.find(str);
    if (it != sUniformLookup.end()) {
        return it->second;
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
