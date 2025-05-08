#include "stdafx.h"
#include "GLTexture.h"


GLTexture::GLTexture(VGTexture handle, vg::TextureTarget type, const ui32v2& dims, i32 mipLevels, vg::TextureFormat format) 
    : mHandle(handle), mType(type), mDims(dims), mMipLevels(mipLevels), mFormat(format) {

}

GLTexture::GLTexture(GLTexture&& o) :
    mType(o.mType),
    mHandle(o.mHandle),
    mHandleBindless(o.mHandleBindless),
    mDims(o.mDims),
    mMipLevels(o.mMipLevels),
    mFormat(o.mFormat)
{
    o.mHandle = 0;
    o.mHandleBindless = 0;
}


GLTexture& GLTexture::operator=(GLTexture&& o) {
    mType = o.mType;
    mHandle = o.mHandle;
    mHandleBindless = o.mHandleBindless;
    mDims = o.mDims;
    mMipLevels = o.mMipLevels;
    mFormat = o.mFormat;

    o.mHandle = 0;
    o.mHandleBindless = 0;
    return *this;
}

GLTexture::~GLTexture() {
    destroy();
}

void GLTexture::init(VGTexture handle, vg::TextureTarget type, const ui32v2& dims, i32 mipLevels, vg::TextureFormat format) {
    destroy();

    mDims = dims;
    mHandle = handle;
    mType = type;
    mMipLevels = mipLevels;
    mFormat = format;
}

void GLTexture::destroy() {
    if (mHandleBindless) {
        glMakeTextureHandleNonResidentARB(mHandleBindless);
        mHandleBindless = 0;
    }
    glDeleteTextures(1, &mHandle);
    mHandle = 0;
}

GLuint64 GLTexture::getHandleBindless() const {
    ASSERT_RENDER_THREAD();
    if (!mHandleBindless) {
        mHandleBindless = glGetTextureHandleARB(mHandle);
        // glGetTextureSamplerHandleARB if we want multiple samplers
        glMakeTextureHandleResidentARB(mHandleBindless);
    }
    return mHandleBindless;
}
