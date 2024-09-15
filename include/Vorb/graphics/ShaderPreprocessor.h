//
// ShaderPreprocessor.h
// Vorb Engine
//
// Created by Benjamin Arnold on 29 Mar 2015
// Refactored on 9/15/2024
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ShaderParser.h
 * @brief Handles parsing of includes for shaders.
 */

#pragma once

#ifndef Vorb_ShaderParser_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ShaderParser_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <map>
#include <set>

#include "Vorb/types.h"
#endif // !VORB_USING_PCH

#include "Vorb/Event.hpp"
#include "Vorb/Vorb.h"
#include "Vorb/VorbPreDecl.inl"
#include "Vorb/graphics/GLEnums.h"
#include "Vorb/graphics/gtypes.h"

#include "ShaderDefine.h"

DECL_VIO(class IOManager);

struct ShaderPreprocessError {
    ShaderPreprocessError(const nString& m, const nString& c) : message(m), code(c) {}

    nString message;
    nString code;
};

namespace vorb {
    namespace graphics {

        class ShaderPreprocessor {
        public:
            /// Parses includes and semantics for a vertex shader
            /// @param inputCode: The input code to use for parsing
            /// @param resultCode: The stored resulting code after parse
            /// @param attributeNames: The stored attribute names
            /// @param semantics: The stored semantics, 1 to 1 with attributeNames
            /// @param iom: Optional iomanager to use for include lookups
            static void processVertexShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom, const ShaderDefinesVector* defines);
            // Parses includes for a fragment shader
            /// @param inputCode: The input code to use for parsing
            /// @param resultCode: The stored resulting code after parse
            /// @param iom: Optional iomanager to use for include lookups
            static void processFragmentOrGeometryShader(const cString inputCode, OUT nString& resultCode, vio::IOManager& iom, const ShaderDefinesVector* defines);
            
            static eventpp::CallbackList<void(const ShaderPreprocessError&)> onError; ///< Event that fires on a parsing error
        private:
            static bool tryParseInclude(nString& s, size_t i);
            static bool checkForComment(const cString s, size_t i);
            static bool tryParseIfdef(nString& s, size_t& i, const ShaderDefinesVector& defines);
           
            static bool isComment() { return isBlockComment || isNormalComment; }

            static std::set<nString> m_parsedIncludes; ///< Cache of already parsed includes to check for circular includes
            static bool isNormalComment; ///< True when in a standard C++ style comment
            static bool isBlockComment; ///< True when in a block comment
            static vio::IOManager* ioManager; ///< IOManager handle to avoid passing
        };
    }
}

#endif // !Vorb_ShaderParser_h__
