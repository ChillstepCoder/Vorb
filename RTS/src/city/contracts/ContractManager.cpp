#include "stdafx.h"
#include "ContractManager.h"

#include "ecs/component/ContractHolderComponent.h"

ContractID sContractIdGen = 0;

ItemShipmentContract* ContractManager::createItemShipmentContract(entt::registry& registry, entt::entity shipper, entt::entity shippee, ItemID itemId, ui16 quantity, TimeSpanSec maxAllowedTime /*= std::numeric_limits<TimeSpanSec>::max()*/) {
    std::unique_ptr<ItemShipmentContract> newContract = std::make_unique<ItemShipmentContract>(shipper, shippee, maxAllowedTime);

    // TODO: Price?
    newContract->mItemId = itemId;
    newContract->mQuantity = quantity;
    newContract->mType = ContractType::ITEM_SHIPMENT;

    ItemShipmentContract* rv = newContract.get();
    registerNewContract(registry, std::move(newContract), shipper, shippee);
    return rv;
}

void ContractManager::endContract(entt::registry& registry, IContract* contract) {
    assert(contract);
    ContractID contractId = contract->mID;

    entt::entity entityA = contract->mEntityA;
    if (entityA != INVALID_ENTITY) {
        ContractHolderComponent& cmp = registry.get<ContractHolderComponent>(entityA);
        cmp.mHeldContracts.erase(contractId);
        // We only have contract holder component when we have a contract
        if (cmp.mHeldContracts.empty()) {
            registry.remove<ContractHolderComponent>(entityA);
        }
    }
    entt::entity entityB = contract->mEntityB;
    if (entityB != INVALID_ENTITY) {
        ContractHolderComponent& cmp = registry.get<ContractHolderComponent>(entityB);
        cmp.mHeldContracts.erase(contractId);
        // We only have contract holder component when we have a contract
        if (cmp.mHeldContracts.empty()) {
            registry.remove<ContractHolderComponent>(entityB);
        }
    }

    auto&& it = sContracts.find(contractId);
    assert(it != sContracts.end());
    sContracts.erase(it);
}

void ContractManager::registerNewContract(entt::registry& registry, std::unique_ptr<IContract>&& contract, entt::entity entityA, entt::entity entityB) {

    // Ensure ID collision is impossible
    auto it = sContracts.find(sContractIdGen);
    while (it != sContracts.end()) {
        it = sContracts.find(++sContractIdGen);
    };
    contract->mID = sContractIdGen;

    registry.get_or_emplace<ContractHolderComponent>(entityA).mHeldContracts.insert(contract->mID);
    registry.get_or_emplace<ContractHolderComponent>(entityB).mHeldContracts.insert(contract->mID);

    sContracts.insert(std::make_pair(sContractIdGen++, std::move(contract)));

    if (sContractIdGen > INT32_MAX) {
        sContractIdGen = 0;
    }
}
