//
// ShaderManager.h
// Vorb Engine
//
// Created by Benjamin Arnold on 29 Mar 2015
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ShaderManager.h
 * @brief Handles caching, creation, and destruction of programs.
 */

#pragma once

#ifndef Vorb_ShaderManager_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ShaderManager_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <map>

#include "../types.h"
#endif // !VORB_USING_PCH

#include "../Event.hpp"
#include "../VorbPreDecl.inl"
#include "../io/Path.h"

#include "../io/IOManager.h"


namespace vorb {
    namespace graphics {

        enum class SHADER_ERROR_EVENT_TYPE {
            FileIOFailure,
            ShaderCompilationError,
            ProgramLinkError
        };

        class GLProgram;
        typedef std::map<nString, GLProgram> GLProgramMap;

        /// Static class that handles caching, creation, and destruction of GLPrograms
        class ShaderManager {
        public:
            static void setMaterialRootDirectory(const vio::Path& rootDir);

            /// TODO(Ben): Add overloads for tesselation, geometry, ect.
            /// Creates a GLProgram from source code.
            /// Does not register to global cache.
            /// @param vertSrc: Source code for vertex shader
            /// @param fragSrc: Source code for fragment shader
            /// @param vertIOM: Optional IOManager for vert #include lookups
            /// @param fragIOM: Optional IOManager for frag #include lookups
            /// @param defines: #defines for the program
            /// @return the created program.
            /// 
            static GLProgram createProgram(
                const cString compSrc, const cString defines
            );
            static GLProgram createProgram(
                const cString vertSrc, const cString fragSrc, const cString defines
            );
            static GLProgram createProgram(
                const cString vertSrc, const cString fragSrc, const cString geomSrc, const cString defines
            );
            static GLProgram createProgram(
                const cString vertSrc, const cString fragSrc, const cString tcsSrc, const cString tesSrc, const cString defines
            );
            /// Creates a GLProgram from files.
            /// Does not register to global cache.
            /// @param vertPath: Path to vertex shader
            /// @param fragPath: Path to fragment shader
            /// @param iom: Optional IOManager for loading
            /// @param defines: #defines for the program
            /// @return the created program.
            static GLProgram createProgramFromFile(const vio::Path& compPath, const cString defines = nullptr);
            static GLProgram createProgramFromFile(const vio::Path& vertPath, const vio::Path& fragPath, const cString defines = nullptr);
            static GLProgram createProgramFromFile(const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path& geometryPath, const cString defines = nullptr);
            static GLProgram createProgramFromFile(const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path& tessControlPath, const vio::Path& tessEvalPath, const cString defines = nullptr);

            /// Disposes and deallocates all globally cached programs and clears the cache
            static void disposeAllPrograms();

            /// Adds a program to the global cache
            /// @param name: String identifier for the program
            /// @param program: The GLProgram to cache
            /// @return false if a program is already cached on that name
            static bool registerProgram(const nString& name, const GLProgram& program);

            /// Removes a program from the global cache and returns it
            /// @param name: String identifier for the program
            /// @return the GLProgram that was unregistered
            static GLProgram unregisterProgram(const nString& name);
            /// Removes a program from the global cache and returns it
            /// WARNING: Is slower than the nString version - O(n) lookup
            /// instead of O(log(n))
            /// @param program: The GLProgram to unregister
            /// @return false if it was not cached
            static bool unregisterProgram(const GLProgram& program);

            /// Gets a program from the cache.
            /// @param name: String identifier for the program
            /// @return nullptr on failure or the program
            static GLProgram& getProgram(const nString& name);

            /// Gets size of program cache
            static GLProgramMap::size_type getNumCachedPrograms() { return m_programMap.size(); };

            /// Gets the read only program map of cached programs
            static const GLProgramMap& getProgramCache() { return m_programMap; }

            static eventpp::EventDispatcher<SHADER_ERROR_EVENT_TYPE, void(const nString&)> errorDispatcher;
        private:
            static void triggerShaderCompilationError(const nString& n); ///< Fires the onShaderCompilationError event
            static void triggerProgramLinkError(const nString& n); ///< Fires the onProgramLinkError event
            static GLProgramMap m_programMap; ///< For globally caching programs
            static GLProgram m_nilProgram;
            static vio::IOManager mIoManager;
        };
    }
}
namespace vg = vorb::graphics;

#endif // !Vorb_ShaderManager_h__
