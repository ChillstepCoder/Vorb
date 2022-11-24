///
/// WindowEventDispatcher.h
/// Vorb Engine
///
/// Created by Cristian Zaloj on 10 Dec 2014
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// Summary:
/// Dispatches events related to the application window handle
///

#pragma once

#ifndef WindowEventDispatcher_h__
#define WindowEventDispatcher_h__

#include "../Event.hpp"


namespace vorb {
    namespace ui {

        enum class WINDOW_EVENT_TYPE {
            Close,
            Resize,
            File,
        };

        struct WindowEvent {
        };

        /// Window resize event data
        struct WindowResizeEvent : public WindowEvent {
            ui32 w = 0u; ///< New window width in pixels
            ui32 h = 0u; ///< New window height in pixels
        };

        /// Window path drop event data
        struct WindowFileEvent : public WindowEvent {
            const cString file = nullptr; ///< Absolute path dropped into the window
        };

        EVENT_DISPATCHER_TYPE(Window, WINDOW_EVENT_TYPE, const WindowEvent&);

        /// Dispatches window events
        class WindowEventManager {
            friend class GameWindow;
            friend class InputDispatcher;
            friend class impl::InputDispatcherEventCatcher;
        public:
            // Listeners
            EVENT_LISTENER_FUNCS(Window, Close, WINDOW_EVENT_TYPE::Close, const WindowEvent&);
            EVENT_LISTENER_FUNCS_ADAPTOR(Window, Resize, WINDOW_EVENT_TYPE::Resize, const WindowResizeEvent&);
            EVENT_LISTENER_FUNCS_ADAPTOR(Window, File, WINDOW_EVENT_TYPE::File, const WindowFileEvent&);

            const ui32v2& getCurrentWindowDims() const { return mCurrentDims; }
            f32 getCurrentAspectRatio() const { return (f32)mCurrentDims.x / (f32)mCurrentDims.y; }
        private:
            ui32v2 mCurrentDims = ui32v2(0);
            EVENT_DISPATCHER(Window);
        };
    }
}
namespace vui = vorb::ui;

#endif // WindowEventDispatcher_h__