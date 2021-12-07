//
// GBuffer.h
// Vorb Engine
//
// Created by Cristian Zaloj on 27 Apr 2015
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file GBuffer.h
 * @brief A special render target used for deferred rendering
 */

#pragma once

#ifndef Vorb_GBuffer_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_GBuffer_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "GLEnums.h"
#include "gtypes.h"

/************************************************************************/
/* GBuffer breakdown                                                    */
/* -----------------------------------------------------------------    */
/* | Diffuse R     | Diffuse G     | Diffuse B     | Light Model   |    */
/* -----------------------------------------------------------------    */
/* | Normal X      | Normal Y      | Normal Z      | Specular Pow  |    */
/* -----------------------------------------------------------------    */
/* | Depth                         |                               |    */
/* -----------------------------------------------------------------    */
/* | Light R       | Light G       | Light B       |  X X X X X X  |    */
/* -----------------------------------------------------------------    */
/************************************************************************/
#define GBUFFER_INTERNAL_FORMAT_COLOR vg::TextureInternalFormat::RGBA16F
#define GBUFFER_INTERNAL_FORMAT_NORMAL vg::TextureInternalFormat::RGBA16F
#define GBUFFER_INTERNAL_FORMAT_DEPTH vg::TextureInternalFormat::RG32F
#define GBUFFER_INTERNAL_FORMAT_LIGHT vg::TextureInternalFormat::RGB16F


enum FboGeometryLayers {
    FBO_GEOMETRY_COLOR = 0,
    FBO_GEOMETRY_NORMAL = 1,
    FBO_GEOMETRY_ROUGHNESS = 2,
    FBO_GEOMETRY_MAX_COUNT = 3
};


namespace vorb {
    namespace graphics {
         /*! @brief Information that specifies size and location of a texture in the GBuffer
          */
        struct GBufferAttachment {
        public:
            vg::TextureInternalFormat format; ///< Internal format for the attachment (all must be the same size).
            vg::TextureFormat pixelFormat;
            vg::TexturePixelType pixelType;
            ui32 number; ///< Attachment index for the texture [0, MaxAttachments).
        };

        /// Geometry and light render target for deferred rendering
        class GBuffer {
        public:
            /// Set up a GBuffer with a certain size
            /// @param w: Width in pixels of each target
            /// @param h: Height in pixels of each target
            GBuffer(ui32 w = 0, ui32 h = 0);
            /// Set up a GBuffer with a certain size
            /// @param s: Size in pixels of each target
            GBuffer(ui32v2 s) : GBuffer(s.x, s.y) {
                // Empty
            }

            /// Create the value-based render targets
            /// @return Self
            GBuffer& init(const GBufferAttachment& geometryAttachment, const GBufferAttachment* normalAttachment, const GBufferAttachment* roughnessAttachment, vg::TextureInternalFormat lightFormat = vg::TextureInternalFormat::NONE, int layerCount = 1);
            /// Attach a depth buffer to this GBuffer
            /// @param depthFormat: Precision used for depth buffer
            /// @return Self
            GBuffer& initDepth(TextureInternalFormat depthFormat = TextureInternalFormat::DEPTH_COMPONENT32, int layerCount = 1);
            /// Attack a depth and stencil buffer to this GBuffer
            /// @param depthFormat: Precision used for depth and stencil buffer
            /// @return Self
            GBuffer& initDepthStencil(TextureInternalFormat depthFormat = TextureInternalFormat::DEPTH24_STENCIL8);

            void initMipLevelsGeom(const vg::GBufferAttachment& geomAttachment, int maxDepth = 0xff);

            void initTarget(const ui32v2& _size, const ui32& texID, const GBufferAttachment& attachment, int layerCount = 1);
            /// Destroy all render targets
            void dispose();

            /// Set up the geometry targets to be active
            void useGeometry();
            /// Set up the light target to be active
            void useLight();

            static void unuse();

            /// Bind Geometry Texture
            /// @param i: Which Geometry texture to bind
            /// @param textureUnit Position to bind texture
            void bindGeometryTexture(ui32 textureUnit, GLenum target = GL_TEXTURE_2D);

            void bindNormalTexture(ui32 textureUnit, GLenum target = GL_TEXTURE_2D);

            /// Bind Depth Texture
            /// @param textureUnit Position to bind texture
            void bindDepthTexture(ui32 textureUnit, GLenum target = GL_TEXTURE_2D);
            
            /// Bind Light Texture
            /// @param textureUnit Position to bind texture
            void bindLightTexture(ui32 textureUnit, GLenum target = GL_TEXTURE_2D);

            /// @return Light texture
            const VGTexture& getGeometryTexture() const {  return m_texGeom;  }
            const VGTexture& getLightTexture() const { return m_texLight; }
            const VGTexture& getNormalTexture() const { return m_texNormal; }
            const VGTexture& getRoughnessTexture() const { return m_texRoughness;  }

            void setSize(ui32 width, ui32 height) {
                m_size.x = width;
                m_size.y = height;
            }
            void setSize(const ui32v2& size) { m_size = size; }

            /// @return Size of the GBuffer in pixels (W,H)
            const ui32v2& getSize() const { return m_size; }
            /// @return Width of the GBuffer in pixels
            const ui32& getWidth() const { return m_size.x; }
            /// @return Height of the GBuffer in pixels
            const ui32& getHeight() const { return m_size.y; }
            const ui32& getNumMipLevels() const { return mMipLevels; }

            const VGFramebuffer& getFboGeometry() const { return m_fboGeom; }
            const VGTexture& getDepthTexture() const { return m_texDepth; }

            void setDepthTexture(VGTexture tex) { m_texDepth = tex; }
            void setLightTexture(VGTexture tex) { m_texLight = tex; }
            void setNormalTexture(VGTexture tex) { m_texNormal = tex; }
            void setRoughnessTexture(VGTexture tex) { m_texRoughness = tex; }

            VGFramebuffer getFboLight() { return m_fboLight; }
            void setFboLight(VGFramebuffer fboLight) { m_fboLight = fboLight; }

            bool checkError();
        private:
            ui32v2 m_size; ///< The width and height of the GBuffer

            VGFramebuffer m_fboGeom = 0; ///< The rendering target for geometry
            VGFramebuffer m_fboLight = 0; ///< The rendering target for light
            VGTexture m_texGeom = 0; ///< Normal texture of GBuffer
            VGTexture m_texNormal = 0; ///< Normal texture of GBuffer
            VGTexture m_texLight = 0; ///< Light texture of GBuffer
            VGTexture m_texDepth = 0; ///< Depth texture of GBuffer
            VGTexture m_texRoughness = 0; ///< Roughness texture of GBuffer
            int mLayerCount = 1;
            ui32 mMipLevels = 0;
        };
    }
}
namespace vg = vorb::graphics;


#endif // !Vorb_GBuffer_h__
