#include "stdafx.h"
#include "Material.h"

#include <Vorb/graphics/Texture.h>


KEG_TYPE_DEF_SAME_NAME(MaterialAtlasTextureInputData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, textureName), keg::BasicType::STRING));
    kt.addValue("unrect", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, uniformRectName), keg::BasicType::STRING));
    kt.addValue("unpage", keg::Value::basic(offsetof(MaterialAtlasTextureInputData, uniformPageName), keg::BasicType::STRING));
}

KEG_TYPE_DEF_SAME_NAME(MaterialData, kt) {
    kt.addValue("atlasTextures", keg::Value::array(offsetof(MaterialData, atlasTextures), keg::Value::custom(0, "MaterialAtlasTextureInputData", false)));
    kt.addValue("vert", keg::Value::basic(offsetof(MaterialData, vertexShaderName), keg::BasicType::STRING));
    kt.addValue("frag", keg::Value::basic(offsetof(MaterialData, fragmentShaderName), keg::BasicType::STRING));
}

const std::map<nString, MaterialUniform> sUniformLookup = {
    std::make_pair("Time", MaterialUniform::Time),
    std::make_pair("TimeOfDay", MaterialUniform::TimeOfDay),
    std::make_pair("SunColor", MaterialUniform::SunColor),
    std::make_pair("SunHeight", MaterialUniform::SunHeight),
    std::make_pair("SunPosition", MaterialUniform::SunPosition),
    std::make_pair("World", MaterialUniform::WMatrix),
    std::make_pair("VP", MaterialUniform::VPMatrix),
    std::make_pair("WVP", MaterialUniform::WVPMatrix),
    std::make_pair("Atlas", MaterialUniform::Atlas),
    std::make_pair("Fbo0", MaterialUniform::Fbo0),
    std::make_pair("FboLight", MaterialUniform::FboLight),
    std::make_pair("FboDepth", MaterialUniform::FboDepth),
    std::make_pair("FboNormals", MaterialUniform::FboNormals),
    std::make_pair("PrevFbo0", MaterialUniform::PrevFbo0),
    std::make_pair("PrevFboDepth", MaterialUniform::PrevFboDepth),
    std::make_pair("PixelDims", MaterialUniform::PixelDims),
    std::make_pair("ZoomScale", MaterialUniform::ZoomScale),
    std::make_pair("FboShadowHeight", MaterialUniform::FboShadowHeight),
};
static_assert((int)MaterialUniform::COUNT == 19, "Update for new material uniform");

extern MaterialUniform lookupMaterialUniform(const nString& str) {
    auto&& it = sUniformLookup.find(str);
    if (it != sUniformLookup.end()) {
        return it->second;
    }
    return MaterialUniform::INVALID;
}

void Material::use() const {

    mProgram.use();

    for (auto&& atlasTextureInput : mInputAtlasTextures) {
        glUniform4fv(atlasTextureInput.uvRectUniform, 1, &atlasTextureInput.uvRect[0]);
        glUniform1fv(atlasTextureInput.pageUniform, 1, &atlasTextureInput.page);
    }
}
