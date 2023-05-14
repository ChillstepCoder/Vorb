#pragma once

#include <gli/texture2d.hpp>

struct TextureUploadInfo {
    vg::TextureInternalFormat internalFormat;
    vg::TexturePixelType texturePixelType;
    vg::TextureFormat textureFormat;
};

namespace TextureHelpers
{
    inline TextureUploadInfo getTextureUploadInfo(const gli::texture2d& texture) {
        TextureUploadInfo info;
        switch (texture.format()) {
            case gli::FORMAT_R8_UNORM_PACK8:
                info.internalFormat = vg::TextureInternalFormat::R8;
                info.texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
                info.textureFormat = vg::TextureFormat::RED;
                break;
            case gli::FORMAT_RG8_UNORM_PACK8:
                info.internalFormat = vg::TextureInternalFormat::RG8;
                info.texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
                info.textureFormat = vg::TextureFormat::RG;
                break;
            case gli::FORMAT_RGB8_UNORM_PACK8:
                info.internalFormat = vg::TextureInternalFormat::RGB8;
                info.texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
                info.textureFormat = vg::TextureFormat::RGB;
                break;
            case gli::FORMAT_RGBA8_UNORM_PACK8:
                info.internalFormat = vg::TextureInternalFormat::RGBA8;
                info.texturePixelType = vg::TexturePixelType::UNSIGNED_BYTE;
                info.textureFormat = vg::TextureFormat::RGBA;
                break;
            default:
                LOG_CRITICAL("Unhandled texture format {} in getTextureUploadInfo", texture.format());
                throw std::exception("Invalid texture format");
        }
        return info;
    }
}