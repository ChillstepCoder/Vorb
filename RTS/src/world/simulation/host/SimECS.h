#pragma once

#include "world/simulation/host/CharacterGroupType.h"

class HostSimContext;
class SimAISystem;
class SimSettlementSystem;

enum class CharacterGroupDissolveReason : ui8 {
    None,
    GoalSuccess,
    Disbanded,
    Merged
};

// Host only, owned by SimThread
class SimECS
{
public:
    SimECS(HostSimContext& hostSimContext);
    ~SimECS();

    void tickSimThread(TimestampMs currentTimestamp);
    // Allocates bare minimum components
    // TODO: entity factory
    entt::entity createNewPerson(f32v2 worldTilePosition);
    entt::registry& getRegistrySimThread() { ASSERT_SIM_THREAD(); return mRegistry; }
    // Returns the group entity
    entt::entity createNewSettlerCaravan(std::span<entt::entity> members, int leaderIndex, f32v2 targetPos);
    // Destroys the group entity and triggers members to resolve the group condition
    void endCharacterGroup(entt::entity group, CharacterGroupDissolveReason reason);

private:
    entt::entity createNewCharacterGroup(std::span<entt::entity> members, int leaderIndex, CharacterGroupType groupType);

    void updateAI();
    void updateSettlements();

    entt::registry mRegistry;
    TimestampMs mCurrentTickTimestamp = 0;
    TimestampMs mTimeDelta = 0;
    TimestampMs mLastTickTimestamp = 0;
    HostSimContext& mHostSimContext;

    // TODO: Serialize this
    CharacterUID mUIDGenerator = 0;

    std::unique_ptr<SimAISystem> mAISystem;
    std::unique_ptr<SimSettlementSystem> mSettlementSystem;
};

