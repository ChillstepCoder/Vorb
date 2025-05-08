#include "stdafx.h"
#include "ShaderLoader.h"

#include <Vorb/Event.hpp>
#include <Vorb/graphics/ShaderManager.h>

#include <conio.h>

typedef eventpp::ScopedRemover<eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const vg::ProgramError&)>> ScopedRemover;

namespace {
    void printShaderError(const vg::ProgramError& n, const nString& name) {
        LOG_INFO("Shader Error: {}\n{}", name.c_str(), n.message.c_str());
    }
    void printLinkError(const vg::ProgramError& n, const nString& name) {
        LOG_INFO("Link Error: {}\n{}", name.c_str(), n.message.c_str());
    }
    void printFileIOError(const vg::ProgramError& n, const nString& name) {
        LOG_INFO("File IO Error: {}\n{}", name.c_str(), n.message.c_str());
    }
}

vg::GLProgram ShaderLoader::createProgram(
    const nString& programName,
    const nString& vertexShaderName,
    const nString& fragmentShaderName,
    const nString geometryShaderName /*= ""*/,
    const nString tessControlShaderName /*= ""*/,
    const nString tessEvalShaderName /*= ""*/,
    const ShaderDefinesVector* defines /* = nullptr*/
) {

    vio::Path vertPath;
    vio::Path fragPath;
    vio::Path geomPath;
    vio::Path tessControlPath;
    vio::Path tessEvalPath;
    if (tessControlShaderName.size()) {
        assert(!geometryShaderName.size());
        assert(tessEvalShaderName.size());
        tryGetCachedPaths(vertexShaderName, fragmentShaderName, tessControlShaderName, tessEvalShaderName, vertPath, fragPath, tessControlPath, tessEvalPath);
    }
    else if (geometryShaderName.size()) {
        tryGetCachedPaths(vertexShaderName, fragmentShaderName, geometryShaderName, vertPath, fragPath, geomPath);
    }
    else {
        tryGetCachedPaths(vertexShaderName, fragmentShaderName, vertPath, fragPath);
    }

    vg::GLProgram newProgram = createProgramFromFile(programName, vertPath, fragPath, geomPath, tessControlPath, tessEvalPath, defines);
    return newProgram;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgramFromFile(
    const nString& programName,
    const vio::Path& vertPath,
    const vio::Path& fragPath,
    const vio::Path geometryPath /*= ""*/,
    const vio::Path tessControlPath /*= ""*/,
    const vio::Path tessEvalPath /*= ""*/,
    const ShaderDefinesVector* defines /*= nullptr*/
) {
    ScopedRemover events(vg::ShaderManager::errorDispatcher);
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [&programName](const vg::ProgramError& s) { printFileIOError(s, programName); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [&programName](const vg::ProgramError& s) { printShaderError(s, programName); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [&programName](const vg::ProgramError& s) { printLinkError(s, programName); });
    
    vg::GLProgram program;
    while (true) {
        if (!tessControlPath.isNull()) {
            assert(!tessEvalPath.isNull());
            assert(geometryPath.isNull());
            program = vg::ShaderManager::createProgramFromFile(vertPath, fragPath, tessControlPath, tessEvalPath, defines);
        }
        else if (!geometryPath.isNull()) {
            // Optional geometry stage
            program = vg::ShaderManager::createProgramFromFile(vertPath, fragPath, geometryPath, defines);
        }
        else {
            program = vg::ShaderManager::createProgramFromFile(vertPath, fragPath, defines);
        }
        if (program.isLinked()) break;
        program.dispose();
        if (geometryPath.isNull()) {
            LOG_CRITICAL("Enter any key to try recompiling with Vertex Shader: {} and Fragment Shader {}\nEnter Z to abort.\n", vertPath.getCString(), fragPath.getCString());
        }
        else {
            LOG_CRITICAL("Enter any key to try recompiling with Vertex Shader: {} and Fragment Shader: {} and Geometry Shader: {}\nEnter Z to abort.\n", vertPath.getCString(), fragPath.getCString(), geometryPath.getCString());
        }
        char tmp = _getch();
        if (tmp == 'Z' || tmp == 'z') break;
    }

    return program;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgram(const nString& name, const cString vertSrc, const cString fragSrc, const ShaderDefinesVector* defines /*= nullptr*/) {
    ScopedRemover events;
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [&name](const vg::ProgramError& s) { printFileIOError(s, name); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [&name](const vg::ProgramError& s) { printShaderError(s, name); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [&name](const vg::ProgramError& s) { printLinkError(s, name); });

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgram(vertSrc, fragSrc, defines);
        if (program.isLinked()) break;
        program.dispose();
        LOG_CRITICAL("Enter any key to try recompiling with {} shader.\nEnter Z to abort.\n", name.c_str());
        char tmp = _getch();
        if (tmp == 'Z' || tmp == 'z') break;
    }

    return program;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createComputeProgramFromFile(const nString& name, const vio::Path& path)
{
    ScopedRemover events(vg::ShaderManager::errorDispatcher);
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [&name](const vg::ProgramError& s) { printFileIOError(s, name); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [&name](const vg::ProgramError& s) { printShaderError(s, name); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [&name](const vg::ProgramError& s) { printLinkError(s, name); });

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgramFromFile(path);
        if (program.isLinked()) break;
        program.dispose();
        LOG_CRITICAL("Enter any key to try recompiling with Compute Shader: {}\nEnter Z to abort.\n", path.getCString());
        char tmp = _getch();
        if (tmp == 'Z' || tmp == 'z') break;
    }

    return program;
}

bool ShaderLoader::refreshFileWriteTime(const nString& shaderName, ShaderType type, OUT fs::file_time_type& inOutTime) {
    switch (type) {
        case ShaderType::Vertex: {
            auto it = sVertexShaderNameToPath.find(shaderName);
            if (it != sVertexShaderNameToPath.end()) {
                std::filesystem::file_time_type time = std::filesystem::last_write_time(it->second.path.getStdPath());
                if (time > inOutTime) {
                    inOutTime = time;
                    return true;
                }
            }
            break;
        }
        case ShaderType::Fragment: {
            auto it = sFragmentShaderNameToPath.find(shaderName);
            if (it != sFragmentShaderNameToPath.end()) {
                std::filesystem::file_time_type time = std::filesystem::last_write_time(it->second.path.getStdPath());
                if (time > inOutTime) {
                    inOutTime = time;
                    return true;
                }
            }
            break;
        }
        case ShaderType::Geometry: {
            auto it = sGeometryShaderNameToPath.find(shaderName);
            if (it != sGeometryShaderNameToPath.end()) {
                std::filesystem::file_time_type time = std::filesystem::last_write_time(it->second.path.getStdPath());
                if (time > inOutTime) {
                    inOutTime = time;
                    return true;
                }
            }
            break;
        }
        case ShaderType::TessControl: {
            auto it = sTessControlShaderNameToPath.find(shaderName);
            if (it != sTessControlShaderNameToPath.end()) {
                std::filesystem::file_time_type time = std::filesystem::last_write_time(it->second.path.getStdPath());
                if (time > inOutTime) {
                    inOutTime = time;
                    return true;
                }
            }
            break;
        }
        case ShaderType::TessEval: {
            auto it = sTessEvalShaderNameToPath.find(shaderName);
            if (it != sTessEvalShaderNameToPath.end()) {
                std::filesystem::file_time_type time = std::filesystem::last_write_time(it->second.path.getStdPath());
                if (time > inOutTime) {
                    inOutTime = time;
                    return true;
                }
            }
            break;
        }
        case ShaderType::Compute: {
            assert(false);
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(e_count(ShaderType) == 6);
    return false;
}

void ShaderLoader::tryGetCachedPaths(const nString& vertexShaderName, const nString& fragmentShaderName, OUT vio::Path& resultVertPath, OUT vio::Path& resultFragPath)
{
    {
        auto&& it = sVertexShaderNameToPath.find(vertexShaderName);
        if (it != sVertexShaderNameToPath.end()) {
            resultVertPath = it->second.path;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second.path;
        }
        else {
            resultFragPath = fragmentShaderName;
        }
    }
}

void ShaderLoader::tryGetCachedPaths(const nString& vertexShaderName, const nString& fragmentShaderName, const nString& geometryShaderName, OUT vio::Path& resultVertPath, OUT vio::Path& resultFragPath, OUT vio::Path& resultGeomPath)
{
    {
        auto&& it = sVertexShaderNameToPath.find(vertexShaderName);
        if (it != sVertexShaderNameToPath.end()) {
            resultVertPath = it->second.path;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second.path;
        }
        else {
            resultFragPath = fragmentShaderName;
        }
    }
    {
        auto&& it = sGeometryShaderNameToPath.find(geometryShaderName);
        if (it != sGeometryShaderNameToPath.end()) {
            resultGeomPath = it->second.path;
        }
        else {
            resultGeomPath = geometryShaderName;
        }
    }
}

void ShaderLoader::tryGetCachedPaths(const nString& vertexShaderName, const nString& fragmentShaderName, const nString& tessControlShaderName, const nString& tessEvalShaderName, OUT vio::Path& resultVertPath, OUT vio::Path& resultFragPath, OUT vio::Path& resultTessControlPath, OUT vio::Path& resultTessEvalPath)
{
    {
        auto&& it = sVertexShaderNameToPath.find(vertexShaderName);
        if (it != sVertexShaderNameToPath.end()) {
            resultVertPath = it->second.path;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second.path;
        }
        else {
            resultFragPath = fragmentShaderName;
        }
    }
    {
        auto&& it = sTessControlShaderNameToPath.find(tessControlShaderName);
        if (it != sTessControlShaderNameToPath.end()) {
            resultTessControlPath = it->second.path;
        }
        else {
            resultTessControlPath = tessControlShaderName;
        }
    }
    {
        auto&& it = sTessEvalShaderNameToPath.find(tessEvalShaderName);
        if (it != sTessEvalShaderNameToPath.end()) {
            resultTessEvalPath = it->second.path;
        }
        else {
            resultTessEvalPath = tessEvalShaderName;
        }
    }
}
