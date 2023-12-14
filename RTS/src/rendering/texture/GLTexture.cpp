#include "stdafx.h"
#include "GLTexture.h"


GLTexture::GLTexture(GLuint handle, vg::TextureTarget type, const ui32v2& dims) : mHandle(handle), mType(type), mDims(dims) {

}

GLTexture::GLTexture(GLTexture&& o) :
    mType(o.mType),
    mHandle(o.mHandle),
    mHandleBindless(o.mHandleBindless),
    mDims(o.mDims)
{
    o.mHandle = 0;
    o.mHandleBindless = 0;
}


GLTexture& GLTexture::operator=(GLTexture&& o)
{
    mType = o.mType;
    mHandle = o.mHandle;
    mHandleBindless = o.mHandleBindless;
    mDims = o.mDims;

    o.mHandle = 0;
    o.mHandleBindless = 0;
    return *this;
}

GLTexture::~GLTexture() {
    destroy();
}

void GLTexture::init(GLuint handle, vg::TextureTarget type, const ui32v2& dims) {
    destroy();

    mDims = dims;
    mHandle = handle;
    mType = type;
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
