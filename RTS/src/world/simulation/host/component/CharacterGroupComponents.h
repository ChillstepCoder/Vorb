#pragma once

#include "ecs/component/InventoryComponent.h"

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

struct CharacterGroupFollowerComponent {
    TimestampMs nextFollowCheckTime = 0; // When to check if we should keep following
    entt::entity groupEntity;
    CharacterGroupFollowerReason followReason = CharacterGroupFollowerReason::None;
};

enum class CharacterGroupType : ui8 {
    Generic,
    SettlerCaravan,
    Combat,
    TradeCaravan,
};

constexpr ui64 CHARACTER_GROUP_DEFAULT_REFRESH_INTERVAL_MS = 20000;
struct CharacterGroupComponent {
    std::vector<entt::entity> groupMembers;
    entt::entity leader = entt::null;
    CharacterGroupType groupType = CharacterGroupType::Generic;
    i32v2 targetPos;
    f32 moveSpeed = 1.0f; // Percentage of default character movement speed
    TimestampMs nextRefreshTime = 0;
};

struct CharacterGroupSharedInventory {
    std::vector<entt::entity> inventoryPoolActors; // Wagons and such
};