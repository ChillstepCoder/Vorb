#pragma once

typedef ui32 SubTextureID;

enum class SubTextureFlags : ui8 {
    HAS_NORMAL_MAP = 1 << 0,
    RAND_FLIP = 1 << 1,
};

struct SubTexture {
    SubTextureID mId;
    VGTexture mTextureDiffuse;
    TextureHandle mTextureHandleDiffuse;
    VGTexture mTextureNormal;
    TextureHandle mTextureHandleNormal;
    f32v4 mUvRect;
    BitFlags<SubTextureFlags> mFlags;
};