#include "stdafx.h"
#include "Cubemap.h"

KEG_TYPE_DEF(CubemapFileData, CubemapFileData, kt) {
    kt.addValue("posx", keg::Value::basic(offsetof(CubemapFileData, mTexPosX), keg::BasicType::STRING));
    kt.addValue("negx", keg::Value::basic(offsetof(CubemapFileData, mTexNegX), keg::BasicType::STRING));
    kt.addValue("posy", keg::Value::basic(offsetof(CubemapFileData, mTexPosY), keg::BasicType::STRING));
    kt.addValue("negy", keg::Value::basic(offsetof(CubemapFileData, mTexNegY), keg::BasicType::STRING));
    kt.addValue("posz", keg::Value::basic(offsetof(CubemapFileData, mTexPosZ), keg::BasicType::STRING));
    kt.addValue("negz", keg::Value::basic(offsetof(CubemapFileData, mTexNegZ), keg::BasicType::STRING));
    kt.addValue("sampler_state", keg::Value::custom(offsetof(CubemapFileData, mSamplerState), "SamplerStateType", true));
}

Cubemap::Cubemap(CubemapID id) : mId(id) {
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mTexture);
}

Cubemap::~Cubemap() {
    glDeleteTextures(1, &mTexture);
}
