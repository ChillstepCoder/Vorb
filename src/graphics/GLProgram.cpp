#include "Vorb/stdafx.h"
#include "Vorb/graphics/GLProgram.h"

#include "Vorb/io/IOManager.h"

// Used for querying attribute and uniforms variables within a program
#define PROGRAM_VARIABLE_IGNORED_PREFIX "gl_"
#define PROGRAM_VARIABLE_IGNORED_PREFIX_LEN 3
#define PROGRAM_VARIABLE_MAX_LENGTH 1024

const vg::ShaderLanguageVersion vg::DEFAULT_SHADING_LANGUAGE_VERSION = vg::ShaderLanguageVersion(
    GL_PROGRAM_DEFAULT_SHADER_VERSION_MAJOR,
    GL_PROGRAM_DEFAULT_SHADER_VERSION_MINOR,
    GL_PROGRAM_DEFAULT_SHADER_VERSION_REVISION
);

VGProgram vg::GLProgram::m_programInUse = 0;

vg::GLProgram::GLProgram(bool init /*= false*/) {
    if (init) this->init();
}

void vg::GLProgram::init() {
    if (isCreated()) return;
    m_isLinked = false;
    m_id = glCreateProgram();
}
void vg::GLProgram::dispose() {
    // Delete the shaders
    if (m_idVS) {
        glDeleteShader(m_idVS);
        m_idVS = 0;
    }
    if (m_idFS) {
        glDeleteShader(m_idFS);
        m_idFS = 0;
    }

    // Delete the program
    if (m_id) {
        glDeleteProgram(m_id);
        m_id = 0;
        m_isLinked = false;
    }
    AttributeMap().swap(m_attributes);
    UniformMap().swap(m_uniforms);
    AttributeSemBinding().swap(m_semanticBinding);
    SsboMap().swap(m_ssboBindings);
}

bool vg::GLProgram::addShader(const ShaderSource& data) {
    // Check current state
    if (isLinked() || !isCreated()) {
        onShaderCompilationError("Cannot add a shader to a fully created or non-existent program");
        return false;
    }

    // Check for preexisting stages
    switch (data.stage) {
        case ShaderType::VERTEX_SHADER:
        case ShaderType::COMPUTE_SHADER:
            if (m_idVS != 0) {
                onShaderCompilationError("Attempting to add another vertex shader");
                return false;
            }
            break;
        case ShaderType::FRAGMENT_SHADER:
            if (m_idFS != 0) {
                onShaderCompilationError("Attempting to add another fragment shader");
                return false;
            }
            break;
        case ShaderType::GEOMETRY_SHADER:
            if (m_idGS != 0) {
                onShaderCompilationError("Attempting to add another fragment shader");
                return false;
            }
            break;
        case ShaderType::TESS_CONTROL_SHADER:
            if (m_idTCS != 0) {
                onShaderCompilationError("Attempting to add another TCS shader");
                return false;
            }
            break;
        case ShaderType::TESS_EVALUATION_SHADER:
            if (m_idTES != 0) {
                onShaderCompilationError("Attempting to add another TES shader");
                return false;
            }
            break;
        default:
            onShaderCompilationError("Shader stage is not supported");
            return false;
    }

    // List of shader code
    const cString* sources = new const cString[data.sources.size() + 1];

    // Version information
    char bufVersion[32];
    sprintf(bufVersion, "#version %d%d%d\n", data.version.major, data.version.minor, data.version.revision);
    sources[0] = bufVersion;

    // Append rest of shader code
    for (size_t i = 0; i < data.sources.size(); i++) {
        sources[i + 1] = data.sources[i];
    }

    // Compile shader
    VGShader idS = glCreateShader((VGEnum)data.stage);
    glShaderSource(idS, (GLsizei)data.sources.size() + 1, sources, 0);
    glCompileShader(idS);
    delete[] sources;

    // Check status
    i32 status;
    glGetShaderiv(idS, GL_COMPILE_STATUS, &status);
    if (status != 1) {
        int infoLogLength;
        glGetShaderiv(idS, GL_INFO_LOG_LENGTH, &infoLogLength);
        std::vector<char> FragmentShaderErrorMessage(infoLogLength);
        glGetShaderInfoLog(idS, infoLogLength, NULL, FragmentShaderErrorMessage.data());
        onShaderCompilationError(FragmentShaderErrorMessage.data());
        glDeleteShader(idS);
        return false;
    }

    // Add shader to stage
    switch (data.stage) {
        case ShaderType::VERTEX_SHADER:
        case ShaderType::COMPUTE_SHADER:
            m_idVS = idS;
            break;
        case ShaderType::FRAGMENT_SHADER:
            m_idFS = idS;
            break;
        case ShaderType::GEOMETRY_SHADER:
            m_idGS = idS;
            break;
        case ShaderType::TESS_CONTROL_SHADER:
            m_idTCS = idS;
            break;
        case ShaderType::TESS_EVALUATION_SHADER:
            m_idTES = idS;
            break;
        default:
            break;
    }
    return true;
}
bool vg::GLProgram::addShader(const ShaderType& type, const cString code, const ShaderLanguageVersion& version /*= DEFAULT_SHADING_LANGUAGE_VERSION*/) {
    ShaderSource src;
    src.stage = type;
    src.sources.push_back(code);
    src.version = version;
    return addShader(src);
}

void vg::GLProgram::setAttribute(nString name, VGAttribute index) {
    // Adding attributes to a linked program does nothing
    if (isLinked() || !isCreated()) return;

    // Set the custom attribute
    glBindAttribLocation(m_id, index, name.c_str());
    m_attributes[name] = index;
}
void vg::GLProgram::setAttributes(const std::map<nString, VGAttribute>& attr) {
    // Adding attributes to a linked program does nothing
    if (isLinked() || !isCreated()) return;

    // Set the custom attributes
    for (auto& binding : attr) {
        glBindAttribLocation(m_id, binding.second, binding.first.c_str());
        m_attributes[binding.first] = binding.second;
    }
}
void vg::GLProgram::setAttributes(const std::vector<AttributeBinding>& attr) {
    // Adding attributes to a linked program does nothing
    if (isLinked() || !isCreated()) return;

    // Set the custom attributes
    for (auto& binding : attr) {
        glBindAttribLocation(m_id, binding.second, binding.first.c_str());
        m_attributes[binding.first] = binding.second;
    }
}
void vg::GLProgram::setAttributes(const std::vector<nString>& attr) {

    // Adding attributes to a linked program does nothing
    if (isLinked() || !isCreated()) return;

    // Set the custom attributes
    for (ui32 i = 0; i < attr.size(); i++) {
        glBindAttribLocation(m_id, i, attr[i].c_str());
        m_attributes[attr[i]] = i;
    }
}

void vg::GLProgram::setAttributes(const std::vector<nString>& attr, const std::vector<VGSemantic>& sem) {
    setAttributes(attr);
    for (int i = 0; i < (int)sem.size(); i++) {
        VGSemantic s = sem[i];
        if (s != Semantic::SEM_INVALID) {
            m_semanticBinding[s] = static_cast<VGAttribute>(i);
        }
    }
}

bool vg::GLProgram::link() {
    // Check internal state
    if (isLinked() || !isCreated()) {
        linkError("Cannot link a fully created or non-existent program");
        return false;
    }

    // Check for available shaders
    if (!m_idVS) {
        linkError("Insufficient stages for a program link");
        return false;
    }

    // Link The Program
    glAttachShader(m_id, m_idVS);
    if (m_idGS) glAttachShader(m_id, m_idGS);
    if (m_idTCS) {
        assert(m_idTES);
        glAttachShader(m_id, m_idTCS);
        glAttachShader(m_id, m_idTES);
    }
    if (m_idFS) glAttachShader(m_id, m_idFS);
    glLinkProgram(m_id);

    // Detach and delete shaders
    glDetachShader(m_id, m_idVS);
    if (m_idGS) {
        glDetachShader(m_id, m_idGS);
        glDeleteShader(m_idGS);
        m_idGS = 0;
    }
    if (m_idTCS) {
        assert(m_idTES);
        glDetachShader(m_id, m_idTCS);
        glDeleteShader(m_idTCS);
        m_idTCS = 0;
        glDetachShader(m_id, m_idTES);
        glDeleteShader(m_idTES);
        m_idTES = 0;
    }
    if (m_idFS) {
        glDetachShader(m_id, m_idFS);
        glDeleteShader(m_idFS);
        m_idFS = 0;
    }
    glDeleteShader(m_idVS);
    m_idVS = 0;

    // Check the link status
    i32 status;
    glGetProgramiv(m_id, GL_LINK_STATUS, &status);
    m_isLinked = status == 1;
    if (!m_isLinked) {
        linkError("Program had link errors");
        return false;
    }
    return true;
}

void vg::GLProgram::initAttributes() {
    if (!isLinked()) return;

    // Obtain attribute count
    i32 count;
    glGetProgramiv(m_id, GL_ACTIVE_ATTRIBUTES, &count);

    // Necessary info
    char name[PROGRAM_VARIABLE_MAX_LENGTH + 1];
    i32 len;
    GLenum type;
    i32 amount;

    // Enumerate through attributes
    for (int i = 0; i < count; i++) {
        // Get attribute info
        glGetActiveAttrib(m_id, i, PROGRAM_VARIABLE_MAX_LENGTH, &len, &amount, &type, name);
        name[len] = 0;
        VGAttribute loc = glGetAttribLocation(m_id, name);

        // Get rid of system attributes
        if (strncmp(name, PROGRAM_VARIABLE_IGNORED_PREFIX, PROGRAM_VARIABLE_IGNORED_PREFIX_LEN) != 0 && loc != -1) {
            m_attributes[name] = loc;
        }
    }
}
void vg::GLProgram::initUniforms() {
    if (!isLinked()) return;

    // Obtain uniform count
    i32 count;
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &count);

    // Necessary info
    char name[PROGRAM_VARIABLE_MAX_LENGTH + 1];
    i32 len;
    GLenum type;
    i32 amount;

    // Enumerate through uniforms
    for (int i = 0; i < count; i++) {
        // Get uniform info
        glGetActiveUniform(m_id, i, PROGRAM_VARIABLE_MAX_LENGTH, &len, &amount, &type, name);
        name[len] = 0;
        VGUniform loc = glGetUniformLocation(m_id, name);

        // Get rid of system uniforms
        if (strncmp(name, PROGRAM_VARIABLE_IGNORED_PREFIX, PROGRAM_VARIABLE_IGNORED_PREFIX_LEN) != 0 && loc != -1) {
            m_uniforms[name] = loc;
        }
    }
}

void vg::GLProgram::initSsboBindings() {
    if (!isLinked()) return;

    // Get the number of active SSBOs
    GLint ssboCount = 0;
    glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &ssboCount);

    // Properties to query
    GLenum properties[] = { GL_NAME_LENGTH, GL_BUFFER_BINDING };

    for (GLint i = 0; i < ssboCount; ++i) {

        // Get the SSBO binding point
        struct {
            GLint nameLength = 0;
            GLint binding = -1;
        } queryData;

        glGetProgramResourceiv(m_id, GL_SHADER_STORAGE_BLOCK, i, 2, properties, 2, nullptr, &queryData.nameLength);

        // Retrieve the SSBO name
        nString nameData;
        nameData.resize(queryData.nameLength);
        glGetProgramResourceName(m_id, GL_SHADER_STORAGE_BLOCK, i, queryData.nameLength + 1, nullptr, nameData.data());

        assert(queryData.binding != -1);
        m_ssboBindings[nameData] = queryData.binding;
    }
}


void vg::GLProgram::bindFragDataLocation(ui32 colorNumber, const char* name) {
    glBindAttribLocation(m_id, colorNumber, name);
}

void vg::GLProgram::enableVertexAttribArrays() const {
    for (auto& attrBind : m_attributes) {
        glEnableVertexAttribArray(attrBind.second);
    }
}

void vg::GLProgram::disableVertexAttribArrays() const {
    for (auto& attrBind : m_attributes) {
        glDisableVertexAttribArray(attrBind.second);
    }
}

void vg::GLProgram::use() const {
    if (!isInUse()) {
        m_programInUse = m_id;
        glUseProgram(m_id);
    }
}

void vg::GLProgram::unuse() {
    if (m_programInUse) {
        m_programInUse = 0;
        glUseProgram(0);
    }
}

void vg::GLProgram::linkError(const nString& s) {
    char buf[256];
    GLsizei len;
    glGetProgramInfoLog(getID(), 255, &len, buf);
    buf[len] = 0;

    nString s2 = s + ": " + buf;
    onProgramLinkError(s2);
}