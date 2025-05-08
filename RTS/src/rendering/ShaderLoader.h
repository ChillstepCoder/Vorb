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
#include <Vorb/graphics/ShaderDefine.h>

DECL_VIO(class IOManager);

enum class ShaderType {
    Vertex,
    Fragment,
    Geometry,
    TessControl,
    TessEval,
    Compute,
    COUNT
};

#pragma once
class ShaderLoader {
public:

    /// Gets or creates a program from two shader paths
    // TODO: c_strings for less heap alloc
    static CALLEE_DELETE vg::GLProgram createProgram(
        const nString& programName,
        const nString& vertexShaderName,
        const nString& fragmentShaderName,
        const nString geometryShaderName = "",
        const nString tessControlShaderName = "",
        const nString tessEvalShaderName = "",
        const ShaderDefinesVector* defines = nullptr
    );

    /// Creates a program using code loaded from files, and does error checking
    /// Does not register with global cache
    static CALLER_DELETE vg::GLProgram createProgramFromFile(
        const nString& programName,
        const vio::Path& vertPath,
        const vio::Path& fragPath,
        const vio::Path geometryPath = "",
        const vio::Path tessControlPath = "",
        const vio::Path tessEvalPath = "",
        const ShaderDefinesVector* defines = nullptr
    );

    /// Creates a program using passed code, and does error checking
    /// Does not register with global cache
    static CALLER_DELETE vg::GLProgram createProgram(const nString& name, const cString vertSrc, const cString fragSrc, const ShaderDefinesVector* defines = nullptr);

    static CALLER_DELETE vg::GLProgram createComputeProgramFromFile(const nString& name, const vio::Path& path);

    static void registerVertexShaderPath(const nString& name, const vio::Path& path) {
        sVertexShaderNameToPath[name] = CachedPath{ path };
    }
    static void registerFragmentShaderPath(const nString& name, const vio::Path& path) {
        sFragmentShaderNameToPath[name] = CachedPath{ path };
    }
    static void registerGeometryShaderPath(const nString& name, const vio::Path& path) {
        sGeometryShaderNameToPath[name] = CachedPath{ path };
    }
    static void registerTessControlShaderPath(const nString& name, const vio::Path& path) {
        sTessControlShaderNameToPath[name] = CachedPath{ path };
    }
    static void registerTessEvalShaderPath(const nString& name, const vio::Path& path) {
        sTessEvalShaderNameToPath[name] = CachedPath{ path };
    } 

    // Return true if it was out of date
    static bool refreshFileWriteTime(const nString& shaderName, ShaderType type, fs::file_time_type& inOutTime);

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

    struct CachedPath {
        vio::Path path;
    };

    inline static std::map<nString, CachedPath> sVertexShaderNameToPath;
    inline static std::map<nString, CachedPath> sFragmentShaderNameToPath;
    inline static std::map<nString, CachedPath> sGeometryShaderNameToPath;
    inline static std::map<nString, CachedPath> sTessControlShaderNameToPath;
    inline static std::map<nString, CachedPath> sTessEvalShaderNameToPath;
};

#endif // ShaderLoader_h__