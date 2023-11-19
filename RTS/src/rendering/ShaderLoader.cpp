#include "stdafx.h"
#include "ShaderLoader.h"

#include <Vorb/Event.hpp>
#include <Vorb/graphics/ShaderManager.h>

std::map<std::pair<nString /*vert*/, nString /*frag*/>, vg::GLProgram> ShaderLoader::sProgramCache;
std::map<nString, vio::Path> ShaderLoader::sVertexShaderNameToPath;
std::map<nString, vio::Path> ShaderLoader::sFragmentShaderNameToPath;
std::map<nString, vio::Path> ShaderLoader::sGeometryShaderNameToPath;
std::map<nString, vio::Path> ShaderLoader::sTessControlShaderNameToPath;
std::map<nString, vio::Path> ShaderLoader::sTessEvalShaderNameToPath;
typedef eventpp::ScopedRemover<eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const nString&)>> ScopedRemover;

namespace {
    void printShaderError(const nString& n) {
        puts("Shader Error: ");
        puts(n.c_str());
    }
    void printLinkError(const nString& n) {
        puts("Link Error: ");
        puts(n.c_str());
    }
    void printFileIOError(const nString& n) {
        puts("FIle IO Error: ");
        puts(n.c_str());
    }
}

vg::GLProgram ShaderLoader::getProgram(const nString& name) {
    return vg::ShaderManager::getProgram(name);
}

vg::GLProgram ShaderLoader::getOrCreateProgram(const nString& vertexShaderName, const nString& fragmentShaderName, const nString geometryShaderName /*= ""*/, const nString tessControlShaderName /*= ""*/, const nString tessEvalShaderName /*= ""*/) {
    
    auto id = std::make_pair(vertexShaderName, fragmentShaderName + geometryShaderName);
    auto&& it = sProgramCache.find(id);
    if (it != sProgramCache.end()) {
        return it->second;
    }

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

    vg::GLProgram newProgram = createProgramFromFile(vertexShaderName + fragmentShaderName + geometryShaderName, vertPath, fragPath, geomPath, tessControlPath, tessEvalPath);
    sProgramCache.insert(std::make_pair(id, newProgram));
    return newProgram;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgramFromFile(const nString& name, const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path geometryPath /*= ""*/, const vio::Path tessControlPath /*= ""*/, const vio::Path tessEvalPath /*= ""*/,
    const cString defines /*= nullptr*/) {
    ScopedRemover events(vg::ShaderManager::errorDispatcher);
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [](const nString& s) { printFileIOError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [](const nString& s) { printShaderError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [](const nString& s) { printLinkError(s); });
    
    assert(!vg::ShaderManager::getProgram(name).isLinked());

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
        char tmp;
        std::cin >> tmp;
        if (tmp == 'Z' || tmp == 'z') break;
    }

    if (program.isLinked()) {
        vg::ShaderManager::registerProgram(name, program);
    }
    return program;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgram(const nString& name, const cString vertSrc, const cString fragSrc, const cString defines /*= nullptr*/) {
    ScopedRemover events;
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [](const nString& s) { printFileIOError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [](const nString& s) { printShaderError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [](const nString& s) { printLinkError(s); });

    assert(!vg::ShaderManager::getProgram(name).isLinked());

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgram(vertSrc, fragSrc, defines);
        if (program.isLinked()) break;
        program.dispose();
        LOG_CRITICAL("Enter any key to try recompiling with {} shader.\nEnter Z to abort.\n", name.c_str());
        char tmp;
        std::cin >> tmp;
        if (tmp == 'Z' || tmp == 'z') break;
    }

    if (program.isLinked()) {
        vg::ShaderManager::registerProgram(name, program);
    }
    return program;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createComputeProgramFromFile(const nString& name, const vio::Path& path)
{
    ScopedRemover events(vg::ShaderManager::errorDispatcher);
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::FileIOFailure, [](const nString& s) { printFileIOError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, [](const nString& s) { printShaderError(s); });
    events.appendListener(vg::SHADER_ERROR_EVENT_TYPE::ProgramLinkError, [](const nString& s) { printLinkError(s); });

    assert(!vg::ShaderManager::getProgram(name).isLinked());

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgramFromFile(path);
        if (program.isLinked()) break;
        program.dispose();
        LOG_CRITICAL("Enter any key to try recompiling with Compute Shader: {}\nEnter Z to abort.\n", path.getCString());
        char tmp;
        std::cin >> tmp;
        if (tmp == 'Z' || tmp == 'z') break;
    }

    if (program.isLinked()) {
        vg::ShaderManager::registerProgram(name, program);
    }
    return program;
}

void ShaderLoader::clearAllCachedPrograms() {
    sProgramCache.clear();
}

void ShaderLoader::clearCachedProgram(nString vert, nString fragGeom) {
    sProgramCache.erase(std::make_pair(vert, fragGeom));
}

void ShaderLoader::tryGetCachedPaths(const nString& vertexShaderName, const nString& fragmentShaderName, OUT vio::Path& resultVertPath, OUT vio::Path& resultFragPath)
{
    {
        auto&& it = sVertexShaderNameToPath.find(vertexShaderName);
        if (it != sVertexShaderNameToPath.end()) {
            resultVertPath = it->second;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second;
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
            resultVertPath = it->second;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second;
        }
        else {
            resultFragPath = fragmentShaderName;
        }
    }
    {
        auto&& it = sGeometryShaderNameToPath.find(geometryShaderName);
        if (it != sGeometryShaderNameToPath.end()) {
            resultGeomPath = it->second;
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
            resultVertPath = it->second;
        }
        else {
            resultVertPath = vertexShaderName;
        }
    }
    {
        auto&& it = sFragmentShaderNameToPath.find(fragmentShaderName);
        if (it != sFragmentShaderNameToPath.end()) {
            resultFragPath = it->second;
        }
        else {
            resultFragPath = fragmentShaderName;
        }
    }
    {
        auto&& it = sTessControlShaderNameToPath.find(tessControlShaderName);
        if (it != sTessControlShaderNameToPath.end()) {
            resultTessControlPath = it->second;
        }
        else {
            resultTessControlPath = tessControlShaderName;
        }
    }
    {
        auto&& it = sTessEvalShaderNameToPath.find(tessEvalShaderName);
        if (it != sTessEvalShaderNameToPath.end()) {
            resultTessEvalPath = it->second;
        }
        else {
            resultTessEvalPath = tessEvalShaderName;
        }
    }
}
