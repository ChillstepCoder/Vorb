//
// Event.hpp
// Vorb Engine
//
// Created by Matthew Marshall on 7 Nov 2018
// Based on the original implementation by Cristian Zaloj
// Copyright 2018 Regrowth Studios
// MIT License
//

/*! \file Event.hpp
 * \brief Provides a definition of events, which can be subscribed to via delegates, and may be
 * triggered, calling each subscriber in turn - in order of subscription.
 */

#pragma once

#ifndef Vorb_Event_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_Event_h__
//! @endcond

/************************************************************************\
 *                     EVENTPP NEW                                      *
\************************************************************************/
#include "eventpp/callbacklist.h"
#include "eventpp/eventdispatcher.h"
#include "eventpp/utilities/scopedremover.h"
#include "eventpp/utilities/argumentadapter.h"

// Define a listener class which has a single object param
#define EVENT_DISPATCHER_TYPE(name, eventType, paramType) \
typedef eventpp::EventDispatcher<eventType, void(paramType)> name##EventDispatcher; \
typedef eventpp::ScopedRemover<eventpp::EventDispatcher<eventType, void(paramType)>> name##Listeners;


#define EVENT_DISPATCHER_DEF(name) \
name##EventDispatcher m##name##EventDispatcher; \
public: \
void register##name##Listeners(name##Listeners& remover) { \
    remover.setDispatcher(m##name##EventDispatcher); \
} \
private:

#define STATIC_EVENT_DISPATCHER_DEF(name) \
inline static name##EventDispatcher s##name##EventDispatcher; \
public: \
static void register##name##Listeners(name##Listeners& remover) { \
    remover.setDispatcher(s##name##EventDispatcher); \
} \
private:

// Create add/remove functions for a specific event
#define EVENT_LISTENER_FUNCS(name, eventName, eventType, paramType) \
[[nodiscard("event handle leak")]] name##EventDispatcher::Handle add##eventName##Listener(const name##EventDispatcher::Callback& callback) { \
    return m##name##EventDispatcher.appendListener(eventType, callback); \
} \
bool add##eventName##Listener(name##Listeners& remover, const name##EventDispatcher::Callback& callback) { \
    return remover.appendListener(eventType, callback); \
} \
void remove##eventName##Listener(const name##EventDispatcher::Handle& handle) { \
    m##name##EventDispatcher.removeListener(eventType, handle); \
} \
void dispatch##eventName##(paramType p) { \
    m##name##EventDispatcher.dispatch(eventType, p); \
}

// Create add/remove functions no param
#define EVENT_LISTENER_FUNCS_VOID(name, eventName, eventType) \
[[nodiscard("event handle leak")]] name##EventDispatcher::Handle add##eventName##Listener(const name##EventDispatcher::Callback& callback) { \
    return m##name##EventDispatcher.appendListener(eventType, callback); \
} \
bool add##eventName##Listener(name##Listeners& remover, const std::function<void()>& callback) { \
    return remover.appendListener(eventType, eventpp::argumentAdapter<void()>(callback)); \
} \
void remove##eventName##Listener(const name##EventDispatcher::Handle& handle) { \
    m##name##EventDispatcher.removeListener(eventType, handle); \
} \
void dispatch##eventName##() { \
    m##name##EventDispatcher.dispatch(eventType); \
}

// Create add/remove functions for a specific event
#define EVENT_LISTENER_FUNCS_ADAPTOR(name, eventName, eventType, paramType) \
[[nodiscard("event handle leak")]] name##EventDispatcher::Handle add##eventName##Listener(const std::function<void(paramType)>& callback) { \
    return m##name##EventDispatcher.appendListener(eventType, eventpp::argumentAdapter<void(paramType)>(callback)); \
} \
bool add##eventName##Listener(name##Listeners& remover, const std::function<void(paramType)>& callback) { \
    return remover.appendListener(eventType, eventpp::argumentAdapter<void(paramType)>(callback)); \
} \
void remove##eventName##Listener(const name##EventDispatcher::Handle& handle) { \
    m##name##EventDispatcher.removeListener(eventType, handle); \
} \
void dispatch##eventName##(paramType p) { \
    m##name##EventDispatcher.dispatch(eventType, p); \
}

// Create add/remove functions for a specific event
#define STATIC_EVENT_LISTENER_FUNCS(name, eventName, eventType, paramType) \
static [[nodiscard("event handle leak")]] name##EventDispatcher::Handle add##eventName##Listener(const name##EventDispatcher::Callback& callback) { \
    return s##name##EventDispatcher.appendListener(eventType, callback); \
} \
static bool add##eventName##Listener(name##Listeners& remover, const name##EventDispatcher::Callback& callback) { \
    return remover.appendListener(eventType, callback); \
} \
static void remove##eventName##Listener(const name##EventDispatcher::Handle& handle) { \
    s##name##EventDispatcher.removeListener(eventType, handle); \
} \
static void dispatch##eventName##(paramType p) { \
    s##name##EventDispatcher.dispatch(eventType, p); \
}

// Create add/remove functions for a specific event
#define STATIC_EVENT_LISTENER_FUNCS_ADAPTOR(name, eventName, eventType, paramType) \
static [[nodiscard("event handle leak")]] name##EventDispatcher::Handle add##eventName##Listener(const std::function<void(paramType)>& callback) { \
    return s##name##EventDispatcher.appendListener(eventType, eventpp::argumentAdapter<void(paramType)>(callback)); \
} \
static bool add##eventName##Listener(name##Listeners& remover, const std::function<void(paramType)>& callback) { \
    return remover.appendListener(eventType, eventpp::argumentAdapter<void(paramType)>(callback)); \
} \
static void remove##eventName##Listener(const name##EventDispatcher::Handle& handle) { \
    s##name##EventDispatcher.removeListener(eventType, handle); \
} \
static void dispatch##eventName##(paramType p) { \
    s##name##EventDispatcher.dispatch(eventType, p); \
}

#endif // !Vorb_Event_h__
