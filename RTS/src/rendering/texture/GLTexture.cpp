#include "stdafx.h"
#include "GLTexture.h"


GLTexture::GLTexture(CALLEE_DELETE GLuint handle, vg::TextureTarget type, const ui32v2& dims) : mHandle(handle), mType(type), mDims(dims) {
    mHandleBindless = glGetTextureHandleARB(handle);
    glMakeTextureHandleResidentARB(mHandleBindless);
}

GLTexture::~GLTexture() {
    if (mHandleBindless) {
        glMakeTextureHandleNonResidentARB(mHandleBindless);
        glDeleteTextures(1, &mHandle);
    }
}

void GLTexture::init(GLuint handle, vg::TextureTarget type, const ui32v2& dims) {
    destroy();

    mDims = dims;
    mHandle = handle;
    mType = type;
    mHandleBindless = glGetTextureHandleARB(handle);
    glMakeTextureHandleResidentARB(mHandleBindless);
}

void GLTexture::destroy() {
    if (mHandleBindless) {
        glMakeTextureHandleNonResidentARB(mHandleBindless);
        mHandleBindless = 0;
        glDeleteTextures(1, &mHandle);
        mHandle = 0;
    }
}
