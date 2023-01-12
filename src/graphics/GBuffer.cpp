#include "Vorb/stdafx.h"
#include "Vorb/graphics/GBuffer.h"

vg::GBuffer::GBuffer(ui32 w, ui32 h, int layerCount) : mSize(w, h), mLayerCount(layerCount) {
    glCreateFramebuffers(1, &mFbo);
}

vg::GBuffer::~GBuffer() {
    dispose();
}

void vg::GBuffer::dispose() {
    if (mFbo) {
        glDeleteFramebuffers(1, &mFbo);
        mFbo = 0;
        for (int i = 0; i < (int)GBufferAttachmentIndex::COUNT; ++i) {
            if (mAttachments[i].mTexture) {
                glDeleteTextures(1, &mAttachments[i].mTexture);
                mAttachments[i].mTexture = 0;
            }
        }
        if (mTexDepth.mTexture) {
            glDeleteTextures(1, &mTexDepth.mTexture);
            mTexDepth.mTexture = 0;
        }
    }
}

vg::GBuffer& vg::GBuffer::initAttachment(GBufferAttachmentIndex index, vg::TextureInternalFormat format, const vg::SamplerState& samplerState /*= vg::sSamplerStates.POINT_CLAMP*/, int mipLevels /*= 1*/) {
    assert(mFbo);
    GBufferAttachmentTexture& attachment = mAttachments[(int)index];

    initTexture(attachment, (VGEnum)format, samplerState, mipLevels);

    glNamedFramebufferTexture(mFbo, GL_COLOR_ATTACHMENT0 + (VGEnum)index, attachment.mTexture, 0);

    // Mark this as a valid render target
    mDrawBuffers[(int)index] = GL_COLOR_ATTACHMENT0 + (VGEnum)index;
    // Just reupload all draw buffers every time for simplicity (glDrawBuffer only allows you to specify a single buffer)
    glNamedFramebufferDrawBuffers(mFbo, (GLsizei)GBufferAttachmentIndex::COUNT, mDrawBuffers);
    checkError();

    return *this;
}

vg::GBuffer& vg::GBuffer::initDepth(GBufferDepthFormat depthFormat, int mipLevels /*= 1*/) {

    initTexture(mTexDepth, (VGEnum)depthFormat, vg::sSamplerStates.POINT_CLAMP, mipLevels);
    glNamedFramebufferTexture(mFbo, GL_DEPTH_ATTACHMENT, mTexDepth.mTexture, 0);

    checkError();
    return *this;
}
//vg::GBuffer& vg::GBuffer::initDepthStencil(TextureInternalFormat depthFormat /*= TextureInternalFormat::DEPTH24_STENCIL8*/) {
//    glCreateTextures(GL_TEXTURE_2D, 1, &mTexDepth);
//    glTextureStorage2D(mTexDepth, 1, (VGEnum)depthFormat, mSize.x, mSize.y);
//    vg::sSamplerStates.POINT_CLAMP.setForTarget(GL_TEXTURE_2D);
//
//    glNamedFramebufferTexture(mFbo, GL_DEPTH_STENCIL_ATTACHMENT, mTexDepth, 0);
//
//    checkError();
//    return *this;
//}

void vg::GBuffer::use() const {
    assert(mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glViewport(0, 0, mSize.x, mSize.y);
}

vg::GBuffer::GBuffer(GBuffer&& o) noexcept {
    memcpy(this, &o, sizeof(vg::GBuffer));
    o.mFbo = 0;
}

void vorb::graphics::GBuffer::unuse() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void vg::GBuffer::bindAlbedoTexture(ui32 textureUnit) {
    VGTexture texture = getAlbedoTexture();
    assert(texture);
    glBindTextureUnit(textureUnit, texture);
}
void vg::GBuffer::bindNormalTexture(ui32 textureUnit) {
    VGTexture texture = getNormalTexture();
    assert(texture);
    glBindTextureUnit(textureUnit, texture);
}
void vg::GBuffer::bindDepthTexture(ui32 textureUnit) {
    VGTexture texture = mTexDepth.mTexture;
    assert(texture);
    glBindTextureUnit(textureUnit, texture);
}

void vg::GBuffer::initTexture(GBufferAttachmentTexture& texture, VGEnum format, const vg::SamplerState& samplerState, int mipLevels) {
    VGTexture& tex = texture.mTexture;
    assert(tex == 0);
    texture.mMipLevels = mipLevels;
    glCreateTextures(mLayerCount > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, 1, &tex);

    if (mLayerCount <= 1) {
        glTextureStorage2D(tex, mipLevels, (VGEnum)format, mSize.x, mSize.y);
    }
    else {
        glTextureStorage3D(tex, mipLevels, (VGEnum)format, mSize.x, mSize.y, mLayerCount);
    }
    samplerState.setForTexture(tex);
    if (mipLevels > 0) {
        glGenerateTextureMipmap(tex);
    }
}

bool vorb::graphics::GBuffer::checkError() {
    const int fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::string errorString;
        switch (fboStatus) {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_UNDEFINED:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            case GL_INVALID_ENUM:
                errorString = "No framebuffer bound";
                break;
            default:
                errorString = "UNKNOWN - " + std::to_string(fboStatus);
                break;
        }
        printf("FBO Error: %s %d\n", errorString.c_str(), fboStatus);
        return true;
    }

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::string errorString;
        switch (error) {
            case GL_INVALID_ENUM:
                errorString = "Framebuffer init error. Error code 1280: GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                errorString = "Framebuffer init error.  Error code 1281: GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                errorString = "Framebuffer init error.  Error code 1282: GL_INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                errorString = "Framebuffer init error.  Error code 1283: GL_STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                errorString = "Framebuffer init error.  Error code 1284: GL_STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                errorString = "Framebuffer init error.  Error code 1285: GL_OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorString = "Framebuffer init error.  Error code 1285: GL_INVALID_FRAMEBUFFER_OPERATION";
                break;
            default:
                errorString = "Framebuffer init error.  Error code " + std::to_string(error) + ": UNKNOWN";
                break;
        }
        printf("FBO Error: %s %d\n", errorString.c_str(), (int)error);
        return true;
    }
    return false;

    return false;
}