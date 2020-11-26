//
// DepthState.h
// Vorb Engine
//
// Created by Benjamin Arnold on 26 Nov 2020
// Copyright 2020 Regrowth Studios
// MIT License
//

/*! \file DepthState.h
 * @brief Changes GPU blend functionality.
 */

#pragma once

#ifndef Vorb_BlendState_h__
 //! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_BlendState_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "GLEnums.h"

#include <Vorb/io/Keg.h>

namespace vorb {
    namespace graphics {

        enum class BlendStateType {
            ALPHA,
            ALPHA_PREMULTIPLIED,
            ADDITIVE,
            REPLACE,
            COUNT
        };
        KEG_ENUM_DECL(BlendStateType);

        class BlendState
        {
        public:
            BlendState(GLenum srcFactor, GLenum dstFactor);

            // Apply State In The Rendering Pipeline
            void set() const;
            static void set(const BlendStateType state);

            GLenum srcFactor;
            GLenum dstFactor;

            // TODO: glBlendEquation?
        };

        // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
        static union BlendStates {

            const vg::BlendState STATE_ARRAY[(int)BlendStateType::COUNT];
            struct {
                const vg::BlendState ALPHA;

                // ONE, ONE_MINUS_SRC_ALPHA
                const vg::BlendState ALPHA_PREMULTIPLIED;

                // SRC_ALPHA, ONE
                const vg::BlendState ADDITIVE;

                // ONE, ZERO
                const vg::BlendState REPLACE;
            };
        } sBlendStates = {
            {
                vg::BlendState(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), // ALPHA
                vg::BlendState(GL_ONE, GL_ONE_MINUS_SRC_ALPHA),       // ALPHA_PREMULTIPLIED
                vg::BlendState(GL_SRC_ALPHA, GL_ONE),                 // ADDITIVE
                vg::BlendState(GL_ONE, GL_ZERO),                      // REPLACE
            }
        };
        static_assert((int)vg::BlendStateType::COUNT == 4, "Add new blend states above");

    }
}
namespace vg = vorb::graphics;

#endif // !Vorb_BlendState_h__