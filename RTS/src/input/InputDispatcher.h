///
/// InputDispatcher.h
/// Vorb Engine
///
/// Created by Cristian Zaloj on 10 Dec 2014
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// Summary:
/// Global class that dispatches OS and device inputs via events
///

#pragma once

#ifndef InputDispatcher_h__
#define InputDispatcher_h__

// Comment out to remove imgui
#define VORB_IMPL_IMGUI

#include "input/KeyboardEventManager.h"
#include "input/MouseEventManager.h"
#include "WindowEventManager.h"

#include "Vorb/Event.hpp"

namespace vorb {
    namespace ui {
        class GameWindow;
     
        namespace impl {
            VORB_INTERNAL class InputDispatcherEventCatcher;
        }

        /// Handles receiving and dispatching important events
        class InputDispatcher {
            friend class impl::InputDispatcherEventCatcher;
        public:
            /// Adds an event listening hook
            /// @param w: The window where events will be generated
            /// @pre: SDL is initialized
            /// @throws std::runtime_error: When this is already initialized
            static void init(GameWindow* w);
            /// Removes the event listener from SDL
            static void dispose();

            // Event injection
            static void injectMouseButtonEvent(i32 x, i32 y, MouseButton button, ui8 clicks, bool pressed);

            static MouseEventManager mouse; ///< Dispatches mouse events
            static KeyboardEventManager key; ///< Dispatches keyboard events
            static WindowEventManager window; ///< Dispatches window events
            static eventpp::CallbackList<void()> onQuit;
        private:
            static GameWindow* m_window; ///< Active window
            volatile static bool m_isInit; ///< Keeps track of initialization status
        };
    }
}
namespace vui = vorb::ui;

#endif // InputDispatcher_h__