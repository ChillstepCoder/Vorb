///
/// InputDispatcherEventCatcher.h
/// Vorb Engine
///
/// Created by Cristian Zaloj on 7 Feb 2015
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// Summary:
/// 
///

#pragma once

#ifndef InputDispatcherEventCatcher_h__
#define InputDispatcherEventCatcher_h__

//#if defined(VORB_OS_WINDOWS)
//#include <SDL/SDL.h>
//#else
#include <SDL2/SDL.h>
//#endif

#include "input/KeyboardEventManager.h"

namespace vorb {
    namespace ui {
        namespace impl {
            class InputDispatcherEventCatcher {
            public:
                static i32 onSDLEvent(void* userData, SDL_Event* e);
            };
        }
    }
}
namespace vui = vorb::ui;

#endif // InputDispatcherEventCatcher_h__
