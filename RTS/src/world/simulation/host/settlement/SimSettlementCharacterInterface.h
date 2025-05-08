#pragma once

class SimSettlementSystem;

// Methods that AI characters can use to interface with settlements
class SimSettlementCharacterInterface {
public:
    SimSettlementCharacterInterface(SimSettlementSystem& system) : mSystem(system) {}

    bool tryRequestHomeForSelfAndFamily(entt::entity characterEntity, entt::entity settlementEntity);
    void makePlotOwnedByEntity(entt::entity entity, SettlementPlotID plotId);

private:
    SimSettlementSystem& mSystem;
};

