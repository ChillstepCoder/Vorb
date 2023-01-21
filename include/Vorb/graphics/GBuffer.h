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

#include "Vorb/graphics/SamplerState.h"

/************************************************************************/
/* Typical GBuffer breakdown                                            */
/* -----------------------------------------------------------------    */
/* | Albedo R      | Albedo G      | Albedo B      | Metallic      |    */
/* -----------------------------------------------------------------    */
/* | Normal X      | Normal Y      | Normal Z      | Roughness     |    */
/* -----------------------------------------------------------------    */
/* | Tertiary                      |                               |    */
/* -----------------------------------------------------------------    */
/* | Depth                         |                               |    */
/* -----------------------------------------------------------------    */
/************************************************************************/

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

        enum class GBufferAttachmentIndex {
            ALBEDO  = 0,
            NORMALS  = 1,
            TERTIARY = 2,
            COUNT
        };

        enum class GBufferDepthFormat {
            DEPTH_16 = GL_DEPTH_COMPONENT16,
            DEPTH_24 = GL_DEPTH_COMPONENT24,
            DEPTH_32 = GL_DEPTH_COMPONENT32,
            DEPTH_32F = GL_DEPTH_COMPONENT32F,
        };

        enum class GBufferDepthStencilFormat {
            DEPTH_24_STENCIL_8 = GL_DEPTH24_STENCIL8
        };

        struct GBufferAttachmentTexture {
            VGTexture mTexture;
            int mMipLevels;
        };

        /// Geometry and light render target for deferred rendering
        class GBuffer {
        public:
            VORB_NON_COPYABLE(GBuffer);
            /// Set up a GBuffer with a certain size
            /// @param w: Width in pixels of each target
            /// @param h: Height in pixels of each target

            GBuffer(ui32 w, ui32 h, int layerCount = 1);
            /// Set up a GBuffer with a certain size
            /// @param s: Size in pixels of each target
            GBuffer(ui32v2 s, int layerCount = 1) : GBuffer(s.x, s.y, layerCount) {}
            GBuffer(GBuffer&& o) noexcept;
            GBuffer& operator=(GBuffer&& o) noexcept;
            ~GBuffer();


            GBuffer& initAttachment(GBufferAttachmentIndex index, vg::TextureInternalFormat format, const vg::SamplerState& samplerState = vg::sSamplerStates.POINT_CLAMP, int mipLevels = 1);
            GBuffer& initDepth(GBufferDepthFormat depthFormat, int mipLevels = 1);
            GBuffer& initDepthStencil(GBufferDepthStencilFormat depthFormat = GBufferDepthStencilFormat::DEPTH_24_STENCIL_8, int mipLevels = 1);
            // Allows us to share depth with other framebuffers, we will NOT delete this depth texture on destruction
            void setSharedDepthTexture(CALLEE_DELETE VGTexture depthTexture);
            void setSharedDepthStencilTexture(CALLEE_DELETE VGTexture depthStencilTexture);

            void clearAttachment(GBufferAttachmentIndex index, const f32v4& newColor = f32v4(0.0f));
            void clearDepth(f32 newDepth = 1.0f);
            void clearDepthStencil(f32 newDepth = 1.0f, GLint newStencil = 0);

            void use() const;
            static void unuse();

            void bindAlbedoTexture(ui32 textureUnit);
            void bindNormalTexture(ui32 textureUnit);
            void bindDepthTexture(ui32 textureUnit);

            VGTexture getAlbedoTexture() const { return mAttachments[(int)GBufferAttachmentIndex::ALBEDO].mTexture;  }
            VGTexture getNormalTexture() const { return mAttachments[(int)GBufferAttachmentIndex::NORMALS].mTexture; }
            VGTexture getTertiaryTexture() const { return mAttachments[(int)GBufferAttachmentIndex::TERTIARY].mTexture; }
            VGTexture getDepthTexture() const { return mTexDepth.mTexture; }
            VGTexture getDepthStencilTexture() const { assert(mHasStencil); return mTexDepth.mTexture; }

            const ui32v2& getSize() const { return mSize; }
            const ui32& getWidth() const { return mSize.x; }
            const ui32& getHeight() const { return mSize.y; }
            const ui32& getNumMipLevels(GBufferAttachmentIndex index) const { return mAttachments[(int)GBufferAttachmentIndex::ALBEDO].mMipLevels; }

            VGFramebuffer getFbo() const { return mFbo; }
            bool hasStencil() const { return mHasStencil; }

            // TODO: This is WRONG its a hack for my shitty auto shader uniforms
            // ~GBuffer() will delete shared textures!!!!
            void setNormalTexture(VGTexture tex) { mAttachments[(int)GBufferAttachmentIndex::NORMALS].mTexture = tex; }
            void setTertiaryTexture(VGTexture tex) { mAttachments[(int)GBufferAttachmentIndex::TERTIARY].mTexture = tex; }

        private:
            void initTexture(GBufferAttachmentTexture& texture, VGEnum format, const vg::SamplerState& samplerState, int mipLevels);
            bool checkError();

            ui32v2 mSize; ///< The width and height of the GBuffer

            VGFramebuffer mFbo = 0; ///< The rendering target for geometry
            GBufferAttachmentTexture mAttachments[(int)GBufferAttachmentIndex::COUNT] = {};
            VGEnum mDrawBuffers[(int)GBufferAttachmentIndex::COUNT] = { GL_NONE, GL_NONE, GL_NONE };
            static_assert((int)GBufferAttachmentIndex::COUNT == 3, "Update brace init");
            GBufferAttachmentTexture mTexDepth = {}; // Depth texture 
            int mLayerCount = 1;
            // TODO: Flags
            bool mSharedDepth = false;
            bool mHasStencil = false;
        };

        // Wrapper that represents a chain of vg::GBuffer
        class SwapChain {
        public:
            VORB_NON_COPYABLE(SwapChain);
            SwapChain(ui32 w, ui32 h, ui32 gBufferCount, int layerCount = 1);
            /// Set up a GBuffer with a certain size
            /// @param s: Size in pixels of each target
            SwapChain(ui32v2 s, ui32 gBufferCount, int layerCount = 1) : SwapChain(s.x, s.y, gBufferCount, layerCount) {}
            ~SwapChain();

            void initAttachment(GBufferAttachmentIndex index, vg::TextureInternalFormat format, const vg::SamplerState& samplerState = vg::sSamplerStates.POINT_CLAMP, int mipLevels = 1);
            void initDepth(GBufferDepthFormat depthFormat, int mipLevels = 1);

            GBuffer& get(ui32 index);
            GBuffer& getPrev();
            GBuffer& getNext();
            GBuffer& use(ui32 index);
            GBuffer& useNext();
            GBuffer& useFirst() { return use(0); }

            const ui32v2& getDims() const;
            ui32 getGBufferCount() const { return mGBuffers.size(); }
            ui32 getIndexCurrent() const { return mCurrent; }
        private:
            std::vector<std::unique_ptr<GBuffer>> mGBuffers;
            ui32 mCurrent = 0;
        };
    }
}
namespace vg = vorb::graphics;


#endif // !Vorb_GBuffer_h__
