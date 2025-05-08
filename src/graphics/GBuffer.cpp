#include "Vorb/stdafx.h"
#include "Vorb/graphics/GBuffer.h"

vg::GBuffer::GBuffer(ui32 w, ui32 h, int layerCount) : mSize(w, h), mLayerCount(layerCount) {
    glCreateFramebuffers(1, &mFbo);
}

vg::GBuffer::~GBuffer() {
    if (mFbo) {
        glDeleteFramebuffers(1, &mFbo);
        for (int i = 0; i < (int)GBufferAttachmentIndex::COUNT; ++i) {
            if (mAttachments[i].mTexture) {
                glDeleteTextures(1, &mAttachments[i].mTexture);
            }
        }
        if (mTexDepth.mTexture && !mSharedDepth) {
            glDeleteTextures(1, &mTexDepth.mTexture);
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
    assert(mFbo);

    initTexture(mTexDepth, (VGEnum)depthFormat, vg::sSamplerStates.POINT_CLAMP, mipLevels);
    glNamedFramebufferTexture(mFbo, GL_DEPTH_ATTACHMENT, mTexDepth.mTexture, 0);

    checkError();
    return *this;
}

vg::GBuffer& vg::GBuffer::initDepthStencil(GBufferDepthStencilFormat depthFormat, int mipLevels /*= 1*/) {
    assert(mFbo);

    // TODO: SEPARATE DEPTH STENCIL https://www.reddit.com/r/opengl/comments/tzx6gs/how_can_i_read_the_stencil_value_in_a_fragment/
    // TODO: Should be using GL_DEPTH_COMPONENT[n] and GL_STENCIL_INDEX[8] for the internal formats when creating the textures
    initTexture(mTexDepth, (VGEnum)depthFormat, vg::sSamplerStates.POINT_CLAMP, mipLevels);
    glNamedFramebufferTexture(mFbo, GL_DEPTH_STENCIL_ATTACHMENT, mTexDepth.mTexture, 0);
    mHasStencil = true;


    checkError();
    return *this;
}

void vg::GBuffer::setSharedDepthTexture(CALLEE_DELETE VGTexture depthTexture) {
    // We can set shared multiple times but not if we called initDepth previously
    assert(mSharedDepth || !mTexDepth.mTexture);
    assert(depthTexture);

    mSharedDepth = true;
    if (mTexDepth.mTexture != depthTexture) {
        mTexDepth.mTexture = depthTexture;
        glNamedFramebufferTexture(mFbo, GL_DEPTH_ATTACHMENT, depthTexture, 0);
    }
}

void vg::GBuffer::setSharedDepthStencilTexture(CALLEE_DELETE VGTexture depthStencilTexture) {
    // We can set shared multiple times but not if we called initDepth previously
    assert(mSharedDepth || !mTexDepth.mTexture);
    assert(depthStencilTexture);
    mHasStencil = true;
    mSharedDepth = true;
    if (mTexDepth.mTexture != depthStencilTexture) {
        mTexDepth.mTexture = depthStencilTexture;
        glNamedFramebufferTexture(mFbo, GL_DEPTH_STENCIL_ATTACHMENT, depthStencilTexture, 0);
    }
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

vg::GBuffer& vg::GBuffer::operator=(GBuffer&& o) noexcept {
    memcpy(this, &o, sizeof(vg::GBuffer));
    o.mFbo = 0;
    return *this;
}

void vg::GBuffer::clearAttachment(GBufferAttachmentIndex index, f32v4 newColor /*= f32v4(0.0f)*/) {
    assert(mAttachments[int(index)].mTexture);
    glClearNamedFramebufferfv(mFbo, GL_COLOR, (GLint)index, &newColor.x);
}

void vg::GBuffer::clearDepth(f32 newDepth /*= 1.0f*/) {
    assert(mTexDepth.mTexture);
    glClearNamedFramebufferfv(mFbo, GL_DEPTH, 0, &newDepth);
}

void vg::GBuffer::clearDepthStencil(f32 newDepth /*= 1.0f*/, GLint newStencil/* = 0*/) {
    assert(mHasStencil);
    assert(mTexDepth.mTexture);
    glClearNamedFramebufferfi(mFbo, GL_DEPTH_STENCIL, 0, newDepth, newStencil);
}

void vg::GBuffer::unuse() {
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
}

vg::SwapChain::SwapChain(ui32 w, ui32 h, ui32 gBufferCount, int layerCount /*= 1*/) {
    // 5 is probably too many
    assert(gBufferCount > 1 && gBufferCount <= 4);
    mGBuffers.resize(gBufferCount);
    for (size_t i = 0; i < mGBuffers.size(); ++i) {
        mGBuffers[i] = std::make_unique<GBuffer>(w, h, layerCount);
    }
}

vg::SwapChain::~SwapChain()
{
}

void vg::SwapChain::initAttachment(GBufferAttachmentIndex index, vg::TextureInternalFormat format, const vg::SamplerState& samplerState /*= vg::sSamplerStates.POINT_CLAMP*/, int mipLevels /*= 1*/) {
    for (auto&& gb : mGBuffers) {
        gb->initAttachment(index, format, samplerState, mipLevels);
    }
}

void vg::SwapChain::initDepth(GBufferDepthFormat depthFormat, int mipLevels /*= 1*/) {
    for (auto&& gb : mGBuffers) {
        gb->initDepth(depthFormat, mipLevels);
    }
}

vg::GBuffer& vg::SwapChain::get(ui32 index) {
    return *mGBuffers[index];
}

vg::GBuffer& vg::SwapChain::getPrev() {
    if (mCurrent == 0) {
        return *mGBuffers.back();
    }
    return *mGBuffers[mCurrent - 1];
}

vg::GBuffer& vg::SwapChain::getNext() {
    return *mGBuffers[(mCurrent + 1) % mGBuffers.size()];
}

vg::GBuffer& vg::SwapChain::use(ui32 index) {
    vg::GBuffer& gb = *mGBuffers[index];
    gb.use();
    mCurrent = index;
    return gb;
}

vg::GBuffer& vg::SwapChain::useNext() {
    return use((mCurrent + 1) % mGBuffers.size());
}

const ui32v2& vg::SwapChain::getDims() const {
    return mGBuffers[0]->getSize();
}
