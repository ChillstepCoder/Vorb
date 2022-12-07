#include "Vorb/stdafx.h"
#include "Vorb/graphics/SamplerState.h"

vg::SamplerStates vg::sSamplerStates = {
    {
        vg::SamplerState(vg::TextureMinFilter::NEAREST, vg::TextureMagFilter::NEAREST, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT), //POINT_WRAP
        vg::SamplerState(vg::TextureMinFilter::NEAREST, vg::TextureMagFilter::NEAREST, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE), //POINT_CLAMP
        vg::SamplerState(vg::TextureMinFilter::LINEAR, vg::TextureMagFilter::LINEAR, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT), //LINEAR_WRAP
        vg::SamplerState(vg::TextureMinFilter::LINEAR, vg::TextureMagFilter::LINEAR, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE), // LINEAR_CLAMP
        vg::SamplerState(vg::TextureMinFilter::LINEAR, vg::TextureMagFilter::LINEAR, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_BORDER, vg::TextureWrapMode::CLAMP_BORDER), //LINEAR_CLAMP_BORDER
        vg::SamplerState(vg::TextureMinFilter::NEAREST_MIPMAP_NEAREST, vg::TextureMagFilter::NEAREST, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT), //POINT_WRAP_MIPMAP
        vg::SamplerState(vg::TextureMinFilter::NEAREST_MIPMAP_NEAREST, vg::TextureMagFilter::NEAREST, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE), //POINT_CLAMP_MIPMAP
        vg::SamplerState(vg::TextureMinFilter::LINEAR_MIPMAP_LINEAR,   vg::TextureMagFilter::LINEAR, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT, vg::TextureWrapMode::REPEAT), //LINEAR_WRAP_MIPMAP
        vg::SamplerState(vg::TextureMinFilter::LINEAR_MIPMAP_LINEAR, vg::TextureMagFilter::LINEAR, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE, vg::TextureWrapMode::CLAMP_EDGE), // LINEAR_CLAMP_MIPMAP
    }
};
static_assert(int(vg::SamplerStateType::COUNT) == 9, "Update with new");

KEG_ENUM_DEF(SamplerStateType, vg::SamplerStateType, kt) {
    kt.addValue("point_wrap", vg::SamplerStateType::POINT_WRAP);
    kt.addValue("point_clamp", vg::SamplerStateType::POINT_CLAMP);
    kt.addValue("linear_wrap", vg::SamplerStateType::LINEAR_WRAP);
    kt.addValue("linear_clamp", vg::SamplerStateType::LINEAR_CLAMP);
    kt.addValue("linear_clamp_border", vg::SamplerStateType::LINEAR_CLAMP_BORDER);
    kt.addValue("point_wrap_mipmap", vg::SamplerStateType::POINT_WRAP_MIPMAP);
    kt.addValue("point_clamp_mipmap", vg::SamplerStateType::POINT_CLAMP_MIPMAP);
    kt.addValue("linear_wrap_mipmap", vg::SamplerStateType::LINEAR_WRAP_MIPMAP);
    kt.addValue("linear_clamp_mipmap", vg::SamplerStateType::LINEAR_CLAMP_MIPMAP);
}
static_assert(int(vg::SamplerStateType::COUNT) == 9, "Update with new");

vg::SamplerState::SamplerState(TextureMinFilter texMinFilter, TextureMagFilter texMagFilter, TextureWrapMode texWrapS, 
                                    TextureWrapMode texWrapT, TextureWrapMode texWrapR) :
    m_minFilter(texMinFilter),
    m_magFilter(texMagFilter),
    m_wrapS(texWrapS),
    m_wrapT(texWrapT),
    m_wrapR(texWrapR) {
    // Empty
}
vg::SamplerState::SamplerState(ui32 texMinFilter, ui32 texMagFilter, ui32 texWrapS, ui32 texWrapT, ui32 texWrapR) :
	SamplerState(static_cast<TextureMinFilter>(texMinFilter), static_cast<TextureMagFilter>(texMagFilter), static_cast<TextureWrapMode>(texWrapS),
                    static_cast<TextureWrapMode>(texWrapT), static_cast<TextureWrapMode>(texWrapR)) {
    // Empty
}

//void vg::SamplerState::initObject() {
//    glGenSamplers(1, &m_id);
//    glSamplerParameteri(m_id, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(m_minFilter));
//    glSamplerParameteri(m_id, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(m_magFilter));
//    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_S,     static_cast<GLenum>(m_wrapS));
//    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_T,     static_cast<GLenum>(m_wrapT));
//    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_R,     static_cast<GLenum>(m_wrapR));
//}
//void vg::SamplerState::initPredefined() {
//    SamplerStates::POINT_WRAP.initObject();
//    SamplerStates::POINT_CLAMP.initObject();
//    SamplerStates::LINEAR_WRAP.initObject();
//    SamplerStates::LINEAR_CLAMP.initObject();
//    SamplerStates::POINT_WRAP_MIPMAP.initObject();
//    SamplerStates::POINT_CLAMP_MIPMAP.initObject();
//    SamplerStates::LINEAR_WRAP_MIPMAP.initObject();
//    SamplerStates::LINEAR_CLAMP_MIPMAP.initObject();
//}

void vg::SamplerState::setForTarget(ui32 textureTarget) const {
    glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(m_magFilter));
    glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(m_minFilter));
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_S,     static_cast<GLenum>(m_wrapS));
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_T,     static_cast<GLenum>(m_wrapT));
    glTexParameteri(textureTarget, GL_TEXTURE_WRAP_R,     static_cast<GLenum>(m_wrapR));
}
//
//void vg::SamplerState::setObject(ui32 textureUnit) const {
//    glBindSampler(textureUnit, m_id);
//}

void vg::SamplerState::setForTexture(VGTexture texture) const {
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(m_magFilter));
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(m_minFilter));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, static_cast<GLenum>(m_wrapS));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, static_cast<GLenum>(m_wrapT));
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, static_cast<GLenum>(m_wrapR));
}
