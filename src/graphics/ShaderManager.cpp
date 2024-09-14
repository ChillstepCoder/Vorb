#include "Vorb/stdafx.h"
#include "Vorb/graphics/ShaderManager.h"
#include "Vorb/graphics/GLProgram.h"
#include "Vorb/graphics/ShaderParser.h"

#include <errno.h>

eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const nString&)> vg::ShaderManager::errorDispatcher;
typedef eventpp::ScopedRemover<eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const nString&)>> ScopedRemover;
vg::GLProgramMap vg::ShaderManager::m_programMap;
vg::GLProgram vg::ShaderManager::m_nilProgram;
vio::IOManager vg::ShaderManager::mIoManager;


void vorb::graphics::ShaderManager::setShaderRootDirectory(const vio::Path& rootDir) {
    mIoManager.setSearchDirectory(rootDir);
}

vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgram(const cString compSrc, const cString defines) {
    std::vector<nString> attributeNames;
    std::vector<VGSemantic> semantics;
    nString parsedCompSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const nString& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const nString& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderParser::parseVertexShader(compSrc, parsedCompSrc, attributeNames, semantics, mIoManager);

    // Create vertex shader
    ShaderSource srcCompute;
    srcCompute.stage = vg::ShaderType::COMPUTE_SHADER;
    if (defines) srcCompute.sources.push_back(defines);
    srcCompute.sources.push_back(parsedCompSrc.c_str());
    if (!program.addShader(srcCompute)) {
        program.dispose();
        return m_nilProgram;
    }

    // Set the attributes
    program.setAttributes(attributeNames, semantics);
    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vg::GLProgram vg::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString defines /*= nullptr*/) {

    std::vector<nString> attributeNames;
    std::vector<VGSemantic> semantics;
    nString parsedVertSrc;
    nString parsedFragSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const nString& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const nString& s) { triggerProgramLinkError(s); });
   
    // Parse vertex shader code
    ShaderParser::parseVertexShader(vertSrc, parsedVertSrc, attributeNames, semantics, mIoManager);

    // Create vertex shader
    ShaderSource srcVert;
    srcVert.stage = vg::ShaderType::VERTEX_SHADER;
    if (defines) srcVert.sources.push_back(defines);
    srcVert.sources.push_back(parsedVertSrc.c_str());
    if (!program.addShader(srcVert)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse fragment shader code
    ShaderParser::parseFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager);

    // Create the fragment shader
    ShaderSource srcFrag;
    srcFrag.stage = vg::ShaderType::FRAGMENT_SHADER;
    if (defines) srcFrag.sources.push_back(defines);
    srcFrag.sources.push_back(parsedFragSrc.c_str());
    if (!program.addShader(srcFrag)) {
        program.dispose();
        return m_nilProgram;
    }

    // Set the attributes
    program.setAttributes(attributeNames, semantics);
    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vg::GLProgram vg::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString geomSrc, const cString defines /*= nullptr*/) {


    std::vector<nString> attributeNames;
    std::vector<VGSemantic> semantics;
    nString parsedVertSrc;
    nString parsedFragSrc;
    nString parsedGeomSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const nString& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const nString& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderParser::parseVertexShader(vertSrc, parsedVertSrc, attributeNames, semantics, mIoManager);

    // Create vertex shader
    ShaderSource srcVert;
    srcVert.stage = vg::ShaderType::VERTEX_SHADER;
    if (defines) srcVert.sources.push_back(defines);
    srcVert.sources.push_back(parsedVertSrc.c_str());
    if (!program.addShader(srcVert)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse fragment shader code
    ShaderParser::parseFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager);

    // Create the fragment shader
    ShaderSource srcFrag;
    srcFrag.stage = vg::ShaderType::FRAGMENT_SHADER;
    if (defines) srcFrag.sources.push_back(defines);
    srcFrag.sources.push_back(parsedFragSrc.c_str());
    if (!program.addShader(srcFrag)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse geometry shader code
    ShaderParser::parseFragmentOrGeometryShader(geomSrc, parsedGeomSrc, mIoManager);

    // Create the geometry shader
    ShaderSource srcGeom;
    srcGeom.stage = vg::ShaderType::GEOMETRY_SHADER;
    if (defines) srcGeom.sources.push_back(defines);
    srcGeom.sources.push_back(parsedGeomSrc.c_str());
    if (!program.addShader(srcGeom)) {
        program.dispose();
        return m_nilProgram;
    }

    // Set the attributes
    program.setAttributes(attributeNames, semantics);
    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString tcsSrc, const cString tesSrc, const cString defines /*= nullptr */) {

    std::vector<nString> attributeNames;
    std::vector<VGSemantic> semantics;
    nString parsedVertSrc;
    nString parsedFragSrc;
    nString parsedTcsSrc;
    nString parsedTesSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const nString& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const nString& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderParser::parseVertexShader(vertSrc, parsedVertSrc, attributeNames, semantics, mIoManager);

    // Create vertex shader
    ShaderSource srcVert;
    srcVert.stage = vg::ShaderType::VERTEX_SHADER;
    if (defines) srcVert.sources.push_back(defines);
    srcVert.sources.push_back(parsedVertSrc.c_str());
    if (!program.addShader(srcVert)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse fragment shader code
    ShaderParser::parseFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager);

    // Create the fragment shader
    ShaderSource srcFrag;
    srcFrag.stage = vg::ShaderType::FRAGMENT_SHADER;
    if (defines) srcFrag.sources.push_back(defines);
    srcFrag.sources.push_back(parsedFragSrc.c_str());
    if (!program.addShader(srcFrag)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse tcs shader code
    ShaderParser::parseFragmentOrGeometryShader(tcsSrc, parsedTcsSrc, mIoManager);

    // Create the tcs shader
    ShaderSource srcTcs;
    srcTcs.stage = vg::ShaderType::TESS_CONTROL_SHADER;
    if (defines) srcTcs.sources.push_back(defines);
    srcTcs.sources.push_back(parsedTcsSrc.c_str());
    if (!program.addShader(srcTcs)) {
        program.dispose();
        return m_nilProgram;
    }

    // Parse tes shader code
    ShaderParser::parseFragmentOrGeometryShader(tesSrc, parsedTesSrc, mIoManager);

    // Create the tes shader
    ShaderSource srcTes;
    srcTes.stage = vg::ShaderType::TESS_EVALUATION_SHADER;
    if (defines) srcTes.sources.push_back(defines);
    srcTes.sources.push_back(parsedTesSrc.c_str());
    if (!program.addShader(srcTes)) {
        program.dispose();
        return m_nilProgram;
    }

    // Set the attributes
    program.setAttributes(attributeNames, semantics);
    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initUniforms();
    program.initSsboBindings();

    return program;
}


vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgramFromFile(const vio::Path& compPath, const cString defines /*= nullptr*/)
{
    vio::Path compSearchDir;

    // Set search dir to same dir as the files
    compSearchDir = compPath;
    compSearchDir--;

    nString compSrc;

    // Load in the files with error checking
    mIoManager.setLocalDirectory(compSearchDir);
    if (!mIoManager.readFileToString(compPath, compSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + compPath.getString());
        return m_nilProgram;
    }

    return createProgram(compSrc.c_str(), defines);
}

vg::GLProgram vg::ShaderManager::createProgramFromFile(const vio::Path& vertPath, const vio::Path& fragPath, const cString defines) {
    vio::Path vertSearchDir;
    vio::Path fragSearchDir;

    // Set search dir to same dir as the files
    vertSearchDir = vertPath;
    fragSearchDir = fragPath;
    vertSearchDir--;
    fragSearchDir--;

    nString vertSrc;
    nString fragSrc;
    
    // Load in the files with error checking
    mIoManager.setLocalDirectory(vertSearchDir);
    if (!mIoManager.readFileToString(vertPath, vertSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + vertPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + fragPath.getString());
        return m_nilProgram;
    }

    return createProgram(vertSrc.c_str(), fragSrc.c_str(), defines);
}


vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgramFromFile(
    const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path& geometryPath, const cString defines)
{
    vio::Path vertSearchDir;
    vio::Path fragSearchDir;
    vio::Path geomSearchDir;

    // Set search dir to same dir as the files
    vertSearchDir = vertPath;
    fragSearchDir = fragPath;
    geomSearchDir = geometryPath;
    vertSearchDir--;
    fragSearchDir--;
    geomSearchDir--;

    nString vertSrc;
    nString fragSrc;
    nString geomSrc;

    // Load in the files with error checking
    mIoManager.setLocalDirectory(vertSearchDir);
    if (!mIoManager.readFileToString(vertPath, vertSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + vertPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + fragPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(geomSearchDir);
    if (!mIoManager.readFileToString(geometryPath, geomSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + geometryPath.getString());
        return m_nilProgram;
    }

    return createProgram(vertSrc.c_str(), fragSrc.c_str(), geomSrc.c_str(), defines);
}

vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgramFromFile(const vio::Path& vertPath, const vio::Path& fragPath, const vio::Path& tessControlPath, const vio::Path& tessEvalPath, const cString defines /*= nullptr*/)
{
    vio::Path vertSearchDir;
    vio::Path fragSearchDir;
    vio::Path tcsSearchDir;
    vio::Path tesSearchDir;

    // Set search dir to same dir as the files
    vertSearchDir = vertPath;
    fragSearchDir = fragPath;
    tcsSearchDir = tessControlPath;
    tesSearchDir = tessEvalPath;
    vertSearchDir--;
    fragSearchDir--;
    tcsSearchDir--;
    tesSearchDir--;

    nString vertSrc;
    nString fragSrc;
    nString tcsSrc;
    nString tesSrc;

    // Load in the files with error checking
    mIoManager.setLocalDirectory(vertSearchDir);
    if (!mIoManager.readFileToString(vertPath, vertSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + vertPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + fragPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(tcsSearchDir);
    if (!mIoManager.readFileToString(tessControlPath, tcsSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + tessControlPath.getString());
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(tesSearchDir);
    if (!mIoManager.readFileToString(tessEvalPath, tesSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, nString(strerror(errno)) + " : " + tessEvalPath.getString());
        return m_nilProgram;
    }

    return createProgram(vertSrc.c_str(), fragSrc.c_str(), tcsSrc.c_str(), tesSrc.c_str(), defines);
}

void vg::ShaderManager::disposeAllPrograms() {
    for (auto& it : m_programMap) {
        it.second.dispose();
    }
    GLProgramMap().swap(m_programMap);
}

void vg::ShaderManager::disposeProgram(const nString& name) {
    auto&& it = m_programMap.find(name);
    assert(it != m_programMap.end());
    it->second.dispose();
    m_programMap.erase(it);
}

bool vg::ShaderManager::registerProgram(const nString& name, const GLProgram& program) {
    auto it = m_programMap.find(name);
    if (it != m_programMap.end()) return false;
    m_programMap[name] = program;
    return true;
}

CALLER_DELETE vg::GLProgram vg::ShaderManager::unregisterProgram(const nString& name) {
    auto it = m_programMap.find(name);
    GLProgram rv = it->second;
    m_programMap.erase(it);
    return rv;
}

bool vg::ShaderManager::unregisterProgram(const GLProgram& program) {
    for (auto it = m_programMap.begin(); it != m_programMap.end(); it++) {
        if (it->second.getID() == program.getID()) {
            m_programMap.erase(it);
            return true;
        }
    }
    return false;
}

vg::GLProgram& vg::ShaderManager::getProgram(const nString& name) {
    auto it = m_programMap.find(name);
    if (it == m_programMap.end()) return m_nilProgram;
    return it->second;
}

void vg::ShaderManager::triggerShaderCompilationError(const nString& n) {
    printf("Shader compilation error: %s\n", n.c_str());
    errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, n);
}

void vg::ShaderManager::triggerProgramLinkError(const nString& n) {
    printf("Shader link error: %s\n", n.c_str());
    errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::ProgramLinkError, n);
}
