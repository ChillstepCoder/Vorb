#pragma once

class World;

enum class WorldEventType {
    OnWorldBeginGameThread,
    OnWorldEndGameThread,
    OnWorldEndRenderThread,
    OnEntityCreated
};

class WorldEvent {
public:
   WorldEvent(World& world) : world(world) {}

   World& world;
};

struct WorldEntityEvent : public WorldEvent {
public:
    WorldEntityEvent(World& world, entt::entity entity) : WorldEvent(world), entity(entity) {}

    entt::entity entity;
};

EVENT_DISPATCHER_TYPE(StaticWorld, WorldEventType, World&);
EVENT_DISPATCHER_TYPE(World, WorldEventType, const WorldEvent&);