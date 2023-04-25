#pragma once

class ItemShipmentContract;
class IContract;

class ContractManager {
public:
    ContractManager();
    ~ContractManager();
    // TODO: ContractFactory?
    ItemShipmentContract* createItemShipmentContract(entt::registry& registry, entt::entity shipper, entt::entity shippee, ItemID itemId, ui16 quantity, TimeSpanSec maxAllowedTime = std::numeric_limits<TimeSpanSec>::max());

    void endContract(entt::registry& registry, IContract* contract);
private:
    void registerNewContract(entt::registry& registry, std::unique_ptr<IContract>&& contract, entt::entity entityA, entt::entity entityB);

    std::unordered_map<ContractID, std::unique_ptr<IContract>> sContracts;
};

