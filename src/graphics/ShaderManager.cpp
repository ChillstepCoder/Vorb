#include "Vorb/stdafx.h"
#include "Vorb/graphics/ShaderManager.h"
#include "Vorb/graphics/GLProgram.h"
#include "Vorb/graphics/ShaderPreprocessor.h"

#include "Vorb/logging/Logger.h"

#include <cctype>

eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const vg::ProgramError&)> vg::ShaderManager::errorDispatcher;
typedef eventpp::ScopedRemover<eventpp::EventDispatcher<vg::SHADER_ERROR_EVENT_TYPE, void(const nString&)>> ScopedRemover;
vg::GLProgramMap vg::ShaderManager::m_programMap;
vg::GLProgram vg::ShaderManager::m_nilProgram;
vio::IOManager vg::ShaderManager::mIoManager;


void vorb::graphics::ShaderManager::setShaderRootDirectory(const vio::Path& rootDir) {
    mIoManager.setSearchDirectory(rootDir);
}

vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgram(const cString compSrc, const cString defines) {
    nString parsedCompSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const vg::ProgramError& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const vg::ProgramError& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderPreprocessor::processVertexShader(compSrc, parsedCompSrc, mIoManager, nullptr);

    // Create vertex shader
    ShaderSource srcCompute;
    srcCompute.stage = vg::ShaderType::COMPUTE_SHADER;
    if (defines) srcCompute.sources.push_back(defines);
    srcCompute.sources.push_back(parsedCompSrc.c_str());
    if (!program.addShader(srcCompute)) {
        program.dispose();
        return m_nilProgram;
    }

    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initAttributes();
    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vg::GLProgram vg::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString defines /*= nullptr*/) {

    nString parsedVertSrc;
    nString parsedFragSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const vg::ProgramError& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const vg::ProgramError& s) { triggerProgramLinkError(s); });
   
    // Parse vertex shader code
    ShaderPreprocessor::processVertexShader(vertSrc, parsedVertSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager, nullptr);

    // Create the fragment shader
    ShaderSource srcFrag;
    srcFrag.stage = vg::ShaderType::FRAGMENT_SHADER;
    if (defines) srcFrag.sources.push_back(defines);
    srcFrag.sources.push_back(parsedFragSrc.c_str());
    if (!program.addShader(srcFrag)) {
        program.dispose();
        return m_nilProgram;
    }

    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initAttributes();
    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vg::GLProgram vg::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString geomSrc, const cString defines /*= nullptr*/) {
    nString parsedVertSrc;
    nString parsedFragSrc;
    nString parsedGeomSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const vg::ProgramError& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const vg::ProgramError& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderPreprocessor::processVertexShader(vertSrc, parsedVertSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(geomSrc, parsedGeomSrc, mIoManager, nullptr);

    // Create the geometry shader
    ShaderSource srcGeom;
    srcGeom.stage = vg::ShaderType::GEOMETRY_SHADER;
    if (defines) srcGeom.sources.push_back(defines);
    srcGeom.sources.push_back(parsedGeomSrc.c_str());
    if (!program.addShader(srcGeom)) {
        program.dispose();
        return m_nilProgram;
    }

    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initAttributes();
    program.initUniforms();
    program.initSsboBindings();

    return program;
}

vorb::graphics::GLProgram vorb::graphics::ShaderManager::createProgram(const cString vertSrc, const cString fragSrc, const cString tcsSrc, const cString tesSrc, const cString defines /*= nullptr */) {

    nString parsedVertSrc;
    nString parsedFragSrc;
    nString parsedTcsSrc;
    nString parsedTesSrc;

    // Allocate program object
    GLProgram program(true);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> shaderCompEvent(vg::GLProgram::onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> linkEvent(vg::GLProgram::onProgramLinkError);
    shaderCompEvent.append([](const vg::ProgramError& s) { triggerShaderCompilationError(s); });
    linkEvent.append([](const vg::ProgramError& s) { triggerProgramLinkError(s); });

    // Parse vertex shader code
    ShaderPreprocessor::processVertexShader(vertSrc, parsedVertSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(fragSrc, parsedFragSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(tcsSrc, parsedTcsSrc, mIoManager, nullptr);

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
    ShaderPreprocessor::processFragmentOrGeometryShader(tesSrc, parsedTesSrc, mIoManager, nullptr);

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
    // Link the program
    if (!program.link()) {
        program.dispose();
        return m_nilProgram;
    }

    program.initAttributes();
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
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + compPath.getString(), ""));
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
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + vertPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + fragPath.getString(), ""));
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
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + vertPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + fragPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(geomSearchDir);
    if (!mIoManager.readFileToString(geometryPath, geomSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + geometryPath.getString(), ""));
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
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + vertPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(fragSearchDir);
    if (!mIoManager.readFileToString(fragPath, fragSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + fragPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(tcsSearchDir);
    if (!mIoManager.readFileToString(tessControlPath, tcsSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + tessControlPath.getString(), ""));
        return m_nilProgram;
    }
    mIoManager.setLocalDirectory(tesSearchDir);
    if (!mIoManager.readFileToString(tessEvalPath, tesSrc)) {
        errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::FileIOFailure, vg::ProgramError(nString(strerror(errno)) + " : " + tessEvalPath.getString(), ""));
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


// Helper
int findNumberInParentheses(const nString& str) {
    bool inParentheses = false;
    nString numberStr;

    for (char c : str) {
        if (c == '(') {
            inParentheses = true;
            numberStr.clear();
        }
        else if (c == ')') {
            if (inParentheses && !numberStr.empty()) {
                try {
                    return std::stoi(numberStr);
                }
                catch (const std::invalid_argument&) {
                    // Not a valid number
                    return -1;
                }
                catch (const std::out_of_range&) {
                    // Number is too large for int
                    return -1;
                }
            }
            inParentheses = false;
        }
        else if (inParentheses) {
            if (std::isdigit(c)) {
                numberStr += c;
            }
            else {
                // Non-digit character inside parentheses
                inParentheses = false;
                numberStr.clear();
            }
        }
    }

    return -1; // No valid number in parentheses found
}

// Helper
nString insertLineNumbers(const nString& code, int errorLine) {
    std::istringstream iss(code);
    std::ostringstream oss;
    nString line;
    size_t lineNumber = 1;

    while (std::getline(iss, line)) {
        char fill = ' ';
        if (lineNumber == errorLine) {
            fill = '>';
        }
        oss << std::setw(6) << std::setfill(fill) << lineNumber << ": " << line << "\n";
        ++lineNumber;
    }

    // Handle the case where the last line doesn't end with a newline
    if (!code.empty() && code.back() != '\n') {
        oss.seekp(-1, std::ios_base::end); // Remove the last newline we added
    }

    return oss.str();
}

void vg::ShaderManager::triggerShaderCompilationError(const vg::ProgramError& n) {

    const int errorLine = findNumberInParentheses(n.message);

    vg::ProgramError improvedError(n.message, insertLineNumbers(n.code, errorLine));

    if (improvedError.code.size() > 0) {
        VORB_LOG_DEBUG("Shader Code:\n {}", improvedError.code.c_str());
    }
    VORB_LOG_CRITICAL("Shader compilation error: {}", improvedError.message.c_str());
    errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::ShaderCompilationError, improvedError);
}

void vg::ShaderManager::triggerProgramLinkError(const vg::ProgramError& n) {

    const int errorLine = findNumberInParentheses(n.message);

    vg::ProgramError improvedError(n.message, insertLineNumbers(n.code, errorLine));

    if (improvedError.code.size() > 0) {
        VORB_LOG_DEBUG("Shader Code:\n {}", improvedError.code.c_str());
    }
    VORB_LOG_CRITICAL("Shader link error: {}", improvedError.message.c_str());
    errorDispatcher.dispatch(SHADER_ERROR_EVENT_TYPE::ProgramLinkError, improvedError);
}
