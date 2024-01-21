#pragma once

#include "ecs/component/InventoryComponent.h"
#include "world/simulation/host/CharacterGroupType.h"

// Leads a group of characters around
struct CharacterGroupLeaderComponent {
    entt::entity group;
};

enum class CharacterGroupFollowerReason : ui8 {
    None,
    Settler,
    Bodyguard,
    COUNT
};

enum class CharacterGroupFormationType : ui8 {
    Line3xN,
    COUNT
};

struct CharacterGroupFollowerComponent {
    TimestampMs nextFollowCheckTime = 0; // When to check if we should keep following
    entt::entity groupEntity;
    ui16 followerIndex;
    CharacterGroupFollowerReason followReason = CharacterGroupFollowerReason::None;
};

constexpr ui64 CHARACTER_GROUP_DEFAULT_FOLLOW_CHECK_INTERVAL_MS = 30000;
constexpr ui64 CHARACTER_GROUP_DEFAULT_REFRESH_INTERVAL_MS = 20000;
struct CharacterGroupComponent {
    std::vector<entt::entity> groupMembers;
    entt::entity leader = entt::null;
    CharacterGroupType groupType = CharacterGroupType::Generic;
    CharacterGroupFormationType formation = CharacterGroupFormationType::Line3xN;
    f32v2 targetPos = f32v2(0.0f);
    f32v2 currentHeading = f32v2(1.0f, 0.0f);
    f32 moveSpeed = 1.0f; // Percentage of default character movement speed
    TimestampMs nextRefreshTime = 0;
};

struct CharacterGroupSharedInventory {
    std::vector<entt::entity> inventoryPoolActors; // Wagons and such
};