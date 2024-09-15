//
// ShaderInterface.h
// Vorb Engine
//
// Created by Benjamin Arnold on 29 Mar 2015
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ShaderInterface.h
 * @brief Interface for communicating semantic information between programs and vertex buffer.
 */

#pragma once

#ifndef Vorb_ShaderInterface_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ShaderInterface_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <vector>
#include <unordered_map>

#include "../types.h"
#endif // !VORB_USING_PCH

#include "GLEnums.h"

namespace vorb {
    namespace graphics {

        class GLProgram;
             
        class ShaderInterface {
        public:
            ShaderInterface() {
                // Empty
            };

            /// Disposes VAO and clears bindings
            void dispose();

            /// Builds the VAO from a GLProgram
            /// @return number of bindings successfully linked
            i32 build(const GLProgram* program);

            /// Enables the VAO for this interface
            void use();

            /// Disables VAO
            static void unuse();
        private:
            VGVertexArray m_vao = 0;
        };
    }
}
namespace vg = vorb::graphics;

#endif // !Vorb_ShaderInterface_h__
