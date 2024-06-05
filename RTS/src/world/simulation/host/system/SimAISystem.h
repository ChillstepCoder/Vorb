#pragma once

#include "ecs/ChunkFullTransitionData.h"

class HostSimContext;
class SimECS;
class World;
class RandomGenerator;

struct SimBrainComponent;
struct SimPositionComponent;
struct SimMovementComponent;
struct DualTaskQueueComponent;

struct SimFamily {
    // First is "head of family"
    std::unique_ptr<entt::entity[]> characters;
    const char* name = "INVALID";
    i32 numCharacters = 0;
    // i32 padding
};

class SimAISystem {
public:
    SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);

    void tick(TimestampMs currentTime, TimestampMs deltaTime);
    // Returns true if this entity is no longer simulating, and has no position or movement
    bool setEntityPosition(entt::entity e, f32v2 newPosition);

    FamilyID createFamily(std::span<entt::entity> entities, const char* name);
    SimFamily& getFamily(FamilyID familyId) { ASSERT_SIM_THREAD(); return mFamilies.at(familyId); }

private:
    void updateCharacterGroups();
    void updateFollowCharacterGroup(entt::entity entity, SimBrainComponent& brain, SimPositionComponent& pos);
    void updateSimCharacter(entt::entity entity);
    void updateSimTask(entt::entity entity, DualTaskQueueComponent& taskCmp);

    World& mWorld;
    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTimeMs = 0;
    f32 mDeltaTimeSec = 0;
    i32 mWorldWidthChunks = 0;
    i32 mWorldWidthTiles = 0;
    RandomGenerator* mRandomGen = nullptr;

    FamilyID mFamilyIDGen = 0;
    std::unordered_map<FamilyID, SimFamily> mFamilies;
};

