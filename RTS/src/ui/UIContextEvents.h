#pragma once

class World;

enum class UIContextEventType {
    EditorWorldSet,
    COUNT
};

struct UIContextEvent {
    union {
        World* mWorld;
    };
    UIContextEventType eventType;
};
EVENT_DISPATCHER_TYPE(UIContext, UIContextEventType, const UIContextEvent&);