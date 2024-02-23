#include "stdafx.h"
#include "SettlementPlanner.h"

#include "world/simulation/host/component/SettlementComponents.h"

SettlementPlanner::SettlementPlanner(entt::registry& registry) : mRegistry(registry) {

}

void SettlementPlanner::updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime) {
    SettlementSimComponent& simCmp = mRegistry.get<SettlementSimComponent>(settlementEntity);
    //SettlementPlannerComponent& plannerCmp = mRegistry.get<SettlementPlannerComponent>(settlementEntity);
    SettlementQuartermasterComponent& quartermasterCmp = mRegistry.get<SettlementQuartermasterComponent>(settlementEntity);
    //SettlementWorkOrdersComponent& workOrdersCmp = mRegistry.get<SettlementWorkOrdersComponent>(settlementEntity);

}
