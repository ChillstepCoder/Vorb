#include "stdafx.h"
#include "ShaderLoader.h"

#include <Vorb/Event.hpp>
#include <Vorb/graphics/ShaderManager.h>

namespace {
    void printShaderError(Sender s VORB_MAYBE_UNUSED, const nString& n) {
        puts("Shader Error: ");
        puts(n.c_str());
    }
    void printLinkError(Sender s VORB_MAYBE_UNUSED, const nString& n) {
        puts("Link Error: ");
        puts(n.c_str());
    }
    void printFileIOError(Sender s VORB_MAYBE_UNUSED, const nString& n) {
        puts("FIle IO Error: ");
        puts(n.c_str());
    }
}

vg::GLProgram ShaderLoader::getProgram(const nString& name) {
    return vg::ShaderManager::getProgram(name);
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgramFromFile(const nString& name, const vio::Path& vertPath, const vio::Path& fragPath,
    vio::IOManager* iom /*= nullptr*/, const cString defines /*= nullptr*/) {
    vg::ShaderManager::onFileIOFailure += makeDelegate(printFileIOError);
    vg::ShaderManager::onShaderCompilationError += makeDelegate(printShaderError);
    vg::ShaderManager::onProgramLinkError += makeDelegate(printLinkError);
    
    assert(!vg::ShaderManager::getProgram(name).isLinked());

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgramFromFile(vertPath, fragPath, iom, defines);
        if (program.isLinked()) break;
        program.dispose();
        printf("Enter any key to try recompiling with Vertex Shader: %s and Fragment Shader %s\nEnter Z to abort.\n", vertPath.getCString(), fragPath.getCString());
        char tmp;
        std::cin >> tmp;
        if (tmp == 'Z' || tmp == 'z') break;
    }

    vg::ShaderManager::onFileIOFailure -= makeDelegate(printFileIOError);
    vg::ShaderManager::onShaderCompilationError -= makeDelegate(printShaderError);
    vg::ShaderManager::onProgramLinkError -= makeDelegate(printLinkError);

    if (program.isLinked()) {
        vg::ShaderManager::registerProgram(name, program);
    }
    return program;
}

CALLER_DELETE vg::GLProgram ShaderLoader::createProgram(const nString& name, const cString vertSrc, const cString fragSrc, vio::IOManager* iom /*= nullptr*/, const cString defines /*= nullptr*/) {
    vg::ShaderManager::onFileIOFailure += makeDelegate(printFileIOError);
    vg::ShaderManager::onShaderCompilationError += makeDelegate(printShaderError);
    vg::ShaderManager::onProgramLinkError += makeDelegate(printLinkError);

    assert(!vg::ShaderManager::getProgram(name).isLinked());

    vg::GLProgram program;
    while (true) {
        program = vg::ShaderManager::createProgram(vertSrc, fragSrc, iom, iom, defines);
        if (program.isLinked()) break;
        program.dispose();
        printf("Enter any key to try recompiling with %s shader.\nEnter Z to abort.\n", name.c_str());
        char tmp;
        std::cin >> tmp;
        if (tmp == 'Z' || tmp == 'z') break;
    }

    vg::ShaderManager::onFileIOFailure -= makeDelegate(printFileIOError);
    vg::ShaderManager::onShaderCompilationError -= makeDelegate(printShaderError);
    vg::ShaderManager::onProgramLinkError -= makeDelegate(printLinkError);

    if (program.isLinked()) {
        vg::ShaderManager::registerProgram(name, program);
    }
    return program;
}