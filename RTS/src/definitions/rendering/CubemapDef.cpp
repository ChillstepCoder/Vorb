#include "stdafx.h"
#include "CubemapDef.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/texture/TextureHelpers.h"
#include "util/TextureUtil.h"

CubemapDef::~CubemapDef() {
    if (mTexture) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
    }
}
