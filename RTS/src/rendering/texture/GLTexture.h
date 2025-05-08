#pragma once

#include <Vorb/graphics/GLEnums.h>

typedef ui32 TextureID;
constexpr ui32 INVALID_TEXTURE_ID = UINT32_MAX;

// Represents a managed RAII opengl texture with a bindless handle
class GLTexture
{
public:
    GLTexture(GLTexture&& o);
    GLTexture& operator=(GLTexture && o);

    GLTexture() = default;
    GLTexture(VGTexture handle, vg::TextureTarget type, const ui32v2& dims, i32 mipLevels, vg::TextureFormat format);
    ~GLTexture();

    VORB_NON_COPYABLE(GLTexture);

    void init(VGTexture handle, vg::TextureTarget type, const ui32v2& dims, i32 mipLevels, vg::TextureFormat format);
    void destroy();

    vg::TextureTarget getType() const { return mType; }
    VGTexture getHandle() const { return mHandle; }
    GLuint64 getHandleBindless() const;
    const ui32v2& getDims() const { return mDims; }
    bool hasHandle() const { return mHandle != 0; }
    i32 getMipLevels() const { return mMipLevels; }
    vg::TextureFormat getFormat() const { return mFormat; }

    bool hasBindlessHandle() const { ASSERT_RENDER_THREAD(); return mHandleBindless != 0; }
    bool isTextureImmutable() const {  return hasBindlessHandle(); }

private:
    vg::TextureTarget mType = vg::TextureTarget::NONE;
    VGTexture mHandle = 0;
    mutable GLuint64 mHandleBindless = 0;
    ui32v2 mDims = ui32v2(0);
    i32 mMipLevels = 0;
    vg::TextureFormat mFormat = vg::TextureFormat::NONE;
};
