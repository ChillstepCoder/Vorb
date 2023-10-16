#pragma once

class World;

enum class WORLD_EVENT_TYPE {
    OnWorldBegin,
    OnWorldEnd
};
EVENT_DISPATCHER_TYPE(World, WORLD_EVENT_TYPE, World&);