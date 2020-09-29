#include "Vorb/stdafx.h"
#include "Vorb/graphics/Texture.h"

void vg::Texture::bind() const {
    glBindTexture(static_cast<GLenum>(textureTarget), id);
}

void vg::Texture::unbind() const {
    glBindTexture(static_cast<GLenum>(textureTarget), 0);
}