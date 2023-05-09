#pragma once

class IWorld;

enum class UIContextEventType {
    EditorWorldSet,
    COUNT
};

struct UIContextEvent {
    union {
        IWorld* mWorld;
    };
    UIContextEventType eventType;
};
EVENT_DISPATCHER_TYPE(UIContext, UIContextEventType, const UIContextEvent&);