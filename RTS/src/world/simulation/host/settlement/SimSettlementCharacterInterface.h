#pragma once

// Static methods that AI characters can use to interface with settlements
class SimSettlementCharacterInterface {
public:
    static bool tryRequestHome(entt::entity characterEntity, entt::entity settlementEntity, entt::registry& registry);
};

