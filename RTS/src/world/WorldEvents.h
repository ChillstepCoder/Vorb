#pragma once

class World;

enum class WORLD_EVENT_TYPE {
    OnWorldBeginGameThread,
    OnWorldEndGameThread,
    OnWorldEndRenderThread
};
EVENT_DISPATCHER_TYPE(World, WORLD_EVENT_TYPE, World&);