#include "stdafx.h"
#include "SimSettlementCharacterInterface.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/system/SimSettlementSystem.h"
#include "world/simulation/host/system/SimAISystem.h"

bool SimSettlementCharacterInterface::tryRequestHomeForSelfAndFamily(entt::entity characterEntity, entt::entity settlementEntity) {
    ASSERT_SIM_THREAD();
    // TODO: Some residents are more important than others
    SettlementPlannerComponent& plannerCmp = mSystem.mRegistry.get<SettlementPlannerComponent>(settlementEntity);
    SimFamilyMemberComponent* memberCmp = mSystem.mRegistry.try_get<SimFamilyMemberComponent>(characterEntity);
    if (memberCmp) {
        SimFamily& family = mSystem.mECS.getAISystem().getFamily(memberCmp->familyId);
        for (i32 i = 0; i < family.numCharacters; ++i) {
            mSystem.mRegistry.get<DualResidentComponent>(family.characters[i]).homeState = SimHomeState::Pending;
        }
        plannerCmp.familiesPendingHomes.emplace_back(memberCmp->familyId);
    }
    else {
        mSystem.mRegistry.get<DualResidentComponent>(characterEntity).homeState = SimHomeState::Pending;
        plannerCmp.singleCharactersPendingHomes.emplace_back(characterEntity);
    }
    // For now always return true
    return true;
}

void SimSettlementCharacterInterface::makePlotOwnedByEntity(entt::entity entity, SettlementPlotID plotId) {
    SimOwnershipComponent& ownerCmp = mSystem.mRegistry.get_or_emplace<SimOwnershipComponent>(entity);
    if (ownerCmp.numOwnedPlots) {
        ++ownerCmp.numOwnedPlots;
        std::unique_ptr<SettlementPlotID[]> newPlotsList = std::make_unique<SettlementPlotID[]>(ownerCmp.numOwnedPlots);
        memcpy(newPlotsList.get(), ownerCmp.ownedPlots.get(), (ownerCmp.numOwnedPlots - 1) * sizeof(SettlementPlotID));
        newPlotsList[ownerCmp.numOwnedPlots - 1] = plotId;
        ownerCmp.ownedPlots = std::move(newPlotsList);
    }
    else {
        ownerCmp.numOwnedPlots = 1;
        ownerCmp.ownedPlots = std::make_unique<SettlementPlotID[]>(1);
        ownerCmp.ownedPlots[0] = plotId;
    }
}
