//
// FullQuadVBO.h
// Vorb Engine
//
// Created by Cristian Zaloj on 6 Nov 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file FullQuadVBO.h
 * @brief A mesh for a fullscreen quad.
 */

#pragma once

#ifndef Vorb_FullQuadVBO_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_FullQuadVBO_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

namespace vorb {
    namespace graphics {
        // Wrapper over common functionality to draw a single triangle across the entire screen
        // This is more efficient than a full quad
        // Assumes your shader will generate the vertex positions
        // const vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));
        // gl_Position = vec4(vertices[gl_VertexID],0,1);
        // fUV = 0.5 * gl_Position.xy + vec2(0.5);
        class FullscreenTriangleVAO {
        public:
            void init();
            void dispose();

            // Issues a single 3 vertex draw command with this empty VAO
            void draw() const;
        private:
            ui32 m_vao = 0; ///< Empty VAO
        };
    }
}
namespace vg = vorb::graphics;

extern vg::FullscreenTriangleVAO sGlobalFullTriangleVAO;

#endif // !Vorb_FullQuadVBO_h__
