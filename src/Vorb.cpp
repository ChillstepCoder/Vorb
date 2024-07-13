#include "Vorb/stdafx.h"
#include "Vorb/Vorb.h"

#include "Vorb/io/filesystem.h"

//#include <FreeImage.h>
#if defined(VORB_IMPL_FONT_SDL)
//#if defined(VORB_OS_WINDOWS)
//#include <TTF/SDL_ttf.h>
//#else
#include <SDL_ttf/SDL_ttf.h>
//#endif
#else
// TODO: FreeType?
#endif

#include "Vorb/graphics/ConnectedTextures.h"
#include "Vorb/graphics/SpriteBatch.h"
#include "Vorb/io/IOManager.h"
#include "Vorb/utils.h"
#include "Vorb/VorbLibs.h"
#include "Vorb/Event.hpp"
#include "Vorb/logging/Logger.h"

void doNothing(void*) { 
    // Empty
}
// std::vector<DelegateBase::Deleter> DelegateBase::m_deleters(1, { doNothing });

namespace vorb {
    // Current system settings
    InitParam currentSettings = InitParam::NONE;
    bool isSystemInitialized(const InitParam& p) {
        return (currentSettings & p) != InitParam::NONE;
    }

    /************************************************************************/
    /* Initializers                                                         */
    /************************************************************************/
    InitParam initGraphics() {
        // Check for previous initialization
        if (isSystemInitialized(InitParam::GRAPHICS)) {
            return InitParam::GRAPHICS;
        }

#if defined(VORB_IMPL_FONT_SDL)
        if (TTF_Init() == -1) {
            printf("TTF_Init Error: %s\n", TTF_GetError());
            return InitParam::NONE;
        }
#else
        // TODO(Cristian): FreeType
#endif
//        FreeImage_Initialise();
        vg::ConnectedTextureHelper::init();
        return InitParam::GRAPHICS;
    }
    InitParam initIO() {
        // Check for previous initialization
        if (isSystemInitialized(InitParam::IO)) {
            return InitParam::IO;
        }

        // Correctly retrieve initial path
//        vio::Path path = fs::initial_path().string();
        vio::Path path = fs::current_path().string(); // Only ever called once so it really is just current path.

        // Set the executable directory
#ifdef VORB_OS_WINDOWS
        {
            nString buf(1024, 0);
            GetModuleFileName(nullptr, &buf[0], 1024 * sizeof(TCHAR));
            path = buf;
            path--;
        }
#else
        // TODO: Investigate options

#endif // VORB_OS_WINDOWS
        if (!path.isValid()) path = "."; // No other option
        vio::IOManager::setExecutableDirectory(path.asCanonical());


        if (IsDebuggerPresent()) {
            VORB_LOG_DEBUG("Debugger detected, setting CWD to current_path");
            // Set the current working directory
            path = fs::current_path().string();
            if (!path.isValid()) path = "."; // No other option
            if (path.isValid()) vio::IOManager::setCurrentWorkingDirectory(path.asCanonical());
        }
        else {
            // No debugger means we are running a packaged build, so CWD should be same as exe
            if (path.isValid()) vio::IOManager::setCurrentWorkingDirectory(path.asCanonical());
        }

#ifdef DEBUG
        printf("Executable Directory:\n    %s\n", vio::IOManager::getExecutableDirectory().getCString());
        printf("Current Working Directory:\n    %s\n\n\n", !vio::IOManager::getCurrentWorkingDirectory().isNull()
            ? vio::IOManager::getCurrentWorkingDirectory().getCString()
            : "None Specified");
#endif // DEBUG

        return InitParam::IO;
    }
   
    /************************************************************************/
    /* Disposers                                                            */
    /************************************************************************/
    InitParam disposeGraphics() {
        // Check for existence
        if (!isSystemInitialized(InitParam::GRAPHICS)) {
            return InitParam::GRAPHICS;
        }

#if defined(VORB_IMPL_FONT_SDL)
        TTF_Quit();
#else
        // TODO(Cristian): FreeType
#endif
//        FreeImage_DeInitialise();
        vg::SpriteBatch::disposeProgram();

        return InitParam::GRAPHICS;
    }
    InitParam disposeIO() {
        // Check for existence
        if (!isSystemInitialized(InitParam::IO)) {
            return InitParam::IO;
        }

        return InitParam::IO;
    }
  
}

vorb::InitParam vorb::init(const InitParam& p) {
#define HAS(v, b) ((v & b) != InitParam::NONE)

    // Initialize logger
    // TODO: Config logging level
    vorb::Logger::init(LoggingLevel::Trace);

    vorb::InitParam succeeded = InitParam::NONE;
    if (HAS(p, InitParam::GRAPHICS)) succeeded |= initGraphics();
    if (HAS(p, InitParam::IO)) succeeded |= initIO();

    // Add system flags
    currentSettings |= succeeded;

    return succeeded;

#undef HAS
}
vorb::InitParam vorb::dispose(const InitParam& p) {
#define HAS(v, b) ((v & b) != InitParam::NONE)

    vorb::InitParam succeeded = InitParam::NONE;
    if (HAS(p, InitParam::GRAPHICS)) succeeded |= disposeGraphics();
    if (HAS(p, InitParam::IO)) succeeded |= disposeIO();

    // Remove system flags
    currentSettings &= (InitParam)(~(ui64)succeeded);

    return succeeded;

#undef HAS
}
