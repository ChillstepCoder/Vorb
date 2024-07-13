//
// KeyboardEventDispatcher.h
// Vorb Engine
//
// Created by Cristian Zaloj on 10 Dec 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file KeyboardEventDispatcher.h
 * @brief Dispatches keyboard events.
 */

#pragma once

#ifndef Vorb_KeyboardEventDispatcher_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_KeyboardEventDispatcher_h__
//! @endcond

#include <array>
#include <atomic>

#include "Vorb/Event.hpp"
#include "Vorb/ui/Keys.inl"

#include "eventpp/eventdispatcher.h"

namespace vorb {
    namespace ui {
        namespace impl {
            class InputDispatcherEventCatcher;
        }

#define NUM_KEY_CODES VKEY_HIGHEST_VALUE

        /// Bit array common key modifiers
        struct KeyModifiers {
        public:
            union {
                struct {
                    bool lShift : 1; ///< Left shift pressed state
                    bool rShift : 1; ///< Right shift pressed state
                    bool lCtrl : 1; ///< Left control pressed state
                    bool rCtrl : 1; ///< Right control pressed state
                    bool lAlt : 1; ///< Left alt pressed state
                    bool rAlt : 1; ///< Right alt pressed state
                    bool lGUI : 1; ///< Left menu pressed state
                    bool rGUI : 1; ///< Right menu pressed state
                };
                struct {
                    ui8 shift : 2; ///< Any of the two shift keys
                    ui8 ctrl : 2; ///< Any of the two control keys
                    ui8 alt : 2; ///< Any of the two alt keys
                    ui8 gui : 2; ///< Any of the two menu keys
                };
                ui8 state; ///< State of all the modifier keys
            };
            bool num : 1;
            bool caps : 1;
        };

        /// Keyboard event data
        struct KeyEvent {
        public:
            ui16 keyCode; ///< Virtually mapped key code
            ui16 scanCode; ///< Physical key code
            KeyModifiers mod; ///< Current modifiers
            ui32 repeatCount; ///< Number of times this event was repeated
            mutable bool wasHandled = false; ///< Set to true if you dont want any future events to process this
        };
        
        /// Text event data
        constexpr i32 MAX_TEXT_EVENT_SIZE = 64;
        struct TextEvent {
        public:
            char text[64]; ///< Provided input text
            wchar_t wtext[32]; ///< Text in wide format
        };

        enum class KEY_EVENT_TYPE : ui8 {
            KeyDown,
            KeyUp
        };

        enum class TEXT_EVENT_TYPE : ui8 {
            Text
        }
        ;
        enum class KEY_FOCUS_EVENT_TYPE : ui8 {
            FocusGained,
            FocusLost,
        };


        EVENT_DISPATCHER_TYPE(KeyFocus, KEY_FOCUS_EVENT_TYPE, void);
        EVENT_DISPATCHER_TYPE(Key, KEY_EVENT_TYPE, const KeyEvent&);
        EVENT_DISPATCHER_TYPE(Text, TEXT_EVENT_TYPE, const TextEvent&);

        /// Dispatches keyboard events
        class KeyboardEventManager {
            friend class InputDispatcher;
            friend class vorb::ui::impl::InputDispatcherEventCatcher;
        public:
            KeyboardEventManager();

            i32 getNumPresses(VirtualKey k) const;
            bool hasFocus() const;

            bool isKeyPressed(VirtualKey k) const;
            
            // Listeners 
            EVENT_LISTENER_FUNCS_VOID(KeyFocus, FocusGained, KEY_FOCUS_EVENT_TYPE::FocusGained);
            EVENT_LISTENER_FUNCS_VOID(KeyFocus, FocusLost, KEY_FOCUS_EVENT_TYPE::FocusLost);
            EVENT_LISTENER_FUNCS(Key, KeyDown, KEY_EVENT_TYPE::KeyDown, const KeyEvent&);
            EVENT_LISTENER_FUNCS(Key, KeyUp, KEY_EVENT_TYPE::KeyUp, const KeyEvent&);
            EVENT_LISTENER_FUNCS(Text, Text, TEXT_EVENT_TYPE::Text, const TextEvent&);

        private:

            void addPress(VirtualKey k);
            void release(VirtualKey k);

            std::atomic_bool m_state[NUM_KEY_CODES]; ///< The pressed state each virtual key

            std::array<std::atomic<i32>, NUM_KEY_CODES> m_presses;
            std::atomic<i32> m_focus = ATOMIC_VAR_INIT(0);

            EVENT_DISPATCHER_DEF(KeyFocus);
            EVENT_DISPATCHER_DEF(Key);
            EVENT_DISPATCHER_DEF(Text);
        };
    }
}
namespace vui = vorb::ui;

#endif // !Vorb_KeyboardEventDispatcher_h__
