#pragma once

class IWorld;

enum class WORLD_EVENT_TYPE {
    OnWorldBegin,
    OnWorldEnd
};
EVENT_DISPATCHER_TYPE(IWorld, WORLD_EVENT_TYPE, IWorld&);