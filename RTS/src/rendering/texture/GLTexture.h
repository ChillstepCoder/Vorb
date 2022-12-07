#pragma once

#include <Vorb/graphics/GLEnums.h>

typedef ui32 TextureID;
constexpr ui32 INVALID_TEXTURE_ID = UINT32_MAX;

// Represents a managed RAII opengl texture with a bindless handle
class GLTexture
{
public:
    VORB_NON_COPYABLE_BUT_MOVABLE(GLTexture);
    GLTexture() = default;
    GLTexture(GLuint handle, vg::TextureTarget type, const ui32v2& dims);
    ~GLTexture();

    void init(GLuint handle, vg::TextureTarget type, const ui32v2& dims);
    void destroy();

    vg::TextureTarget getType() const { return mType; }
    GLuint getHandle() const { return mHandle; }
    GLuint64 getHandleBindless() const { return mHandleBindless; }
    const ui32v2& getDims() const { return mDims; }
    bool isValid() const { return mHandleBindless != 0; }

private:
    vg::TextureTarget mType = vg::TextureTarget::NONE;
    GLuint mHandle = 0;
    GLuint64 mHandleBindless = 0;
    ui32v2 mDims = ui32v2(0);
};
