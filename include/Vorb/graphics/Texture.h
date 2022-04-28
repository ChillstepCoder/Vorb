//
// Texture.h
// Vorb Engine
//
// Created by Ben Arnold on 20 Oct 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file Texture.h
 * @brief This file provides a wrapper for a texture.
 */

#pragma once

#ifndef Vorb_Texture_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_Texture_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "gtypes.h"
#include "GLEnums.h"

namespace vorb {
    namespace graphics {
        /// Wrapper struct for a texture
        class Texture {
        public:
            Texture() {};
            Texture(VGTexture id,
                    ui32 w,
                    ui32 h,
                    vg::TextureTarget target = vg::TextureTarget::TEXTURE_2D) :
                textureTarget(target),
                id(id),
                width(w),
                height(h) {
                // Empty
            }
            void bind() const;
            void unbind() const;

            vg::TextureTarget textureTarget;
            VGTexture id = 0; ///< OpenGL texture ID
            union {
                struct {
                    ui32 width; ///< Texture width in pixels
                    ui32 height; ///< Texture height in pixels
                };
                ui32v2 dims;
            };
        };
    }
}
namespace vg = vorb::graphics;

#endif // !Vorb_Texture_h__
