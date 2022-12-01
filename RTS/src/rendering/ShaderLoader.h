///
/// ShaderLoader.h
/// Seed of Andromeda
///
/// Created by Benjamin Arnold on 31 Mar 2015
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// Summary:
/// Handles easy loading and error checking of shaders for SoA
///

#pragma once

#ifndef ShaderLoader_h__
#define ShaderLoader_h__

#include <Vorb/VorbPreDecl.inl>
#include <Vorb/graphics/GLProgram.h>

DECL_VIO(class IOManager);

#pragma once
class ShaderLoader {
public:

    /// Gets a previously created program by name
    static vg::GLProgram getProgram(const nString& name);


    /// Gets or creates a program from two shader paths
    // TODO: c_strings for less heap alloc
    static CALLEE_DELETE vg::GLProgram getOrCreateProgram(const nString& vertexShaderName, const nString& fragmentShaderName, const nString geometryShaderName = "", const nString tessControlShaderName = "", const nString tessEvalShaderName = "");

    /// Creates a program using code loaded from files, and does error checking
    /// Does not register with global cache
    static CALLER_DELETE vg::GLProgram createProgramFromFile(const nString& name, const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path geometryPath = "", const vio::Path tessControlPath = "", const vio::Path tessEvalPath = "",
        const cString defines = nullptr);

    /// Creates a program using passed code, and does error checking
    /// Does not register with global cache
    static CALLER_DELETE vg::GLProgram createProgram(const nString& name, const cString vertSrc, const cString fragSrc, const cString defines = nullptr);

    static CALLER_DELETE vg::GLProgram createComputeProgramFromFile(const nString& name, const vio::Path& path);

    static void registerVertexShaderPath(const nString& name, const vio::Path& path) {
        sVertexShaderNameToPath[name] = path;
    }
    static void registerFragmentShaderPath(const nString& name, const vio::Path& path) {
        sFragmentShaderNameToPath[name] = path;
    }
    static void registerGeometryShaderPath(const nString& name, const vio::Path& path) {
        sGeometryShaderNameToPath[name] = path;
    }
    static void registerTessControlShaderPath(const nString& name, const vio::Path& path) {
        sTessControlShaderNameToPath[name] = path;
    }
    static void registerTessEvalShaderPath(const nString& name, const vio::Path& path) {
        sTessEvalShaderNameToPath[name] = path;
    }

    static void clearAllCachedPrograms();

private:
    static void tryGetCachedPaths(
        const nString& vertexShaderName,
        const nString& fragmentShaderName,
        OUT vio::Path& resultVertPath,
        OUT vio::Path& resultFragPath
    );
    static void tryGetCachedPaths(
        const nString& vertexShaderName,
        const nString& fragmentShaderName,
        const nString& geometryShaderName,
        OUT vio::Path& resultVertPath,
        OUT vio::Path& resultFragPath,
        OUT vio::Path& resultGeomPath
    );
    static void tryGetCachedPaths(
        const nString& vertexShaderName,
        const nString& fragmentShaderName,
        const nString& tessControlShaderName,
        const nString& tessEvalShaderName,
        OUT vio::Path& resultVertPath,
        OUT vio::Path& resultFragPath,
        OUT vio::Path& resultTessControlPath,
        OUT vio::Path& resultTessEvalPath
    );

    static std::map<std::pair<nString /*vert*/, nString /*frag + geom*/>, vg::GLProgram> sProgramCache;
    static std::map<nString, vio::Path> sVertexShaderNameToPath;
    static std::map<nString, vio::Path> sFragmentShaderNameToPath;
    static std::map<nString, vio::Path> sGeometryShaderNameToPath;
    static std::map<nString, vio::Path> sTessControlShaderNameToPath;
    static std::map<nString, vio::Path> sTessEvalShaderNameToPath;
};

#endif // ShaderLoader_h__