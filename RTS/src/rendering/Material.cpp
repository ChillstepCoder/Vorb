#include "stdafx.h"
#include "Material.h"

#include <Vorb/graphics/Texture.h>



KEG_TYPE_DEF_SAME_NAME(MaterialData, kt) {
    kt.addValue("inputTextures", keg::Value::array(offsetof(MaterialData, inputTextureNames), keg::BasicType::STRING));
    kt.addValue("shader", keg::Value::basic(offsetof(MaterialData, shaderName), keg::BasicType::STRING));
}

const std::map<nString, MaterialUniform> sUniformLookup = {
    std::make_pair("Time", MaterialUniform::Time),
    std::make_pair("World", MaterialUniform::WMatrix),
    std::make_pair("VP", MaterialUniform::VPMatrix),
    std::make_pair("WVP", MaterialUniform::WVPMatrix),
    std::make_pair("Atlas", MaterialUniform::Atlas),
};
static_assert((int)MaterialUniform::COUNT == 6, "Update for new material uniform");

extern MaterialUniform lookupMaterialUniform(const nString& str) {
    auto&& it = sUniformLookup.find(str);
    if (it != sUniformLookup.end()) {
        return it->second;
    }
    return MaterialUniform::INVALID;
}

void Material::use() const {

    mProgram.use();
    for (int i = 0; i < mInputTextures.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        mInputTextures[i].bind();
    }

}
