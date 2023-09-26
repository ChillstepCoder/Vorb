#include "stdafx.h"
#include "MaterialShaderDef.h"

#include <Vorb/graphics/Texture.h>

void MaterialShaderDef::use(OUT ui32& nextAvailableTextureIndex) const {
    assert(!mIsCompute);
    mProgram.use();

    for (auto&& textureInput : mInputTextures) {
        glBindTextureUnit(nextAvailableTextureIndex, textureInput.texture);
        glUniform1i(textureInput.textureUniform, nextAvailableTextureIndex++);
    }
}

void MaterialShaderDef::useCompute() const {
    assert(mIsCompute);
    mProgram.use();
}
//
//void MaterialShaderDef::dispose() {
//    mUniforms.clear();
//    mInputTextures.clear();
//}
