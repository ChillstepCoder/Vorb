//
// SamplerState.h
// Vorb Engine
//
// Created by Cristian Zaloj on 22 Jan 2015
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file SamplerState.h
 * \brief Changes GPU texture sampling state.
 */

#pragma once

#ifndef Vorb_SamplerState_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_SamplerState_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "GLEnums.h"
#include "gtypes.h"

#include "SamplerStateType.h"

namespace vorb {
	namespace graphics {
        // TODO: Why are these here?
		#ifndef GL_UNSIGNED_SHORT_5_6_5
		#define GL_UNSIGNED_SHORT_5_6_5 0x8363
		#endif
		#ifndef GL_CLAMP_TO_EDGE
		#define GL_CLAMP_TO_EDGE 0x812F
		#endif

		class SamplerState {
		public:
		    SamplerState(TextureMinFilter texMinFilter, TextureMagFilter texMagFilter, TextureWrapMode texWrapS, 
							TextureWrapMode texWrapT, TextureWrapMode texWrapR);
		    SamplerState(ui32 texMinFilter, ui32 texMagFilter, ui32 texWrapS, ui32 texWrapT, ui32 texWrapR);
		
		    //void initObject();
		    // Initialize All The Sampler Objects When OpenGL Context Is Created
		   // static void initPredefined();
		
		    // Target Is Of The Family GL_TEXTURE_2D/3D/etc. For Use On A Texture
		    void set(ui32 textureTarget) const;
		    // Unit Is In The Range [0 - GraphicsDeviceProperties::maxTextureUnits)
		   // void setObject(ui32 textureUnit) const;
		
		private:
		    //VGSampler m_id;
		
		    TextureMinFilter m_minFilter;
		    TextureMagFilter m_magFilter;
		    TextureWrapMode  m_wrapS;
		    TextureWrapMode  m_wrapT;
		    TextureWrapMode  m_wrapR;
		};

        union SamplerStates {
            const vg::SamplerState STATE_ARRAY[(int)SamplerStateType::COUNT];
            struct {
                const vg::SamplerState POINT_WRAP;
                const vg::SamplerState POINT_CLAMP;
                const vg::SamplerState LINEAR_WRAP;
                const vg::SamplerState LINEAR_CLAMP;
                const vg::SamplerState LINEAR_CLAMP_BORDER;
                const vg::SamplerState POINT_WRAP_MIPMAP;
                const vg::SamplerState POINT_CLAMP_MIPMAP;
                const vg::SamplerState LINEAR_WRAP_MIPMAP;
                const vg::SamplerState LINEAR_CLAMP_MIPMAP;
            };
        };
        static_assert(int(vg::SamplerStateType::COUNT) == 9, "Update with new");
		extern SamplerStates sSamplerStates;
	}
}
namespace vg = vorb::graphics;

#endif // !Vorb_SamplerState_h__
