#pragma once


typedef ui32 SubTextureID;

enum class SubTextureFlags : ui8 {
    HAS_NORMAL_MAP = 1 << 0,
    RAND_FLIP = 1 << 1,
};

struct SubTexture {
    TextureHandle mTextureHandleDiffuse;
    TextureHandle mTextureHandleNormal;
    VGTexture mTextureDiffuse;
    VGTexture mTextureNormal;
    f32v4 mUvRect;
    SubTextureID mId;
    BitFlags<SubTextureFlags> mFlags;
};