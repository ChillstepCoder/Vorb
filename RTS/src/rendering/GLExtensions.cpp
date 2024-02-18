#include "stdafx.h"
#include "GLExtensions.h"

GLExtensions sGlExtensions;
std::set<nString> GLExtensions::sExtensions;

void GLExtensions::init() {
    // No double init
    if (sExtensions.size()) {
        return;
    }

    GLint numExtensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i) {
        const char* str = (const char*)glGetStringi(GL_EXTENSIONS, i);
        sExtensions.insert(str);
    }

    // Shader5 for bindless textures
    if (!hasExtension("GL_ARB_gpu_shader5")) {
        panic("GL_ARB_gpu_shader5 not supported by this GPU. Try updating drivers");
    }

    if (!hasExtension("GL_ARB_bindless_texture")) {
        panic("GL_ARB_bindless_texture not supported by this GPU. Try updating drivers");
    }

    if (!hasExtension("GL_EXT_texture_compression_s3tc")) {
        panic("GL_EXT_texture_compression_s3tc not supported by this GPU. Try updating drivers");
    }

    if (!hasExtension("GL_ARB_gpu_shader_int64")) {
        panic("GL_ARB_gpu_shader_int64 not supported by this GPU. Try updating drivers");
    }
}

bool GLExtensions::hasExtension(const char* extension) {
    return sExtensions.find(extension) != sExtensions.end();
}

