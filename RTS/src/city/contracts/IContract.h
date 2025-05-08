#pragma once

enum class ContractEndReason : ui8 {
    COMPLETED,
    ENTITY_DEATH,
    ENTITY_UNWILLING,
    ENTITY_UNABLE,
    TIME_EXPIRE,
};

enum class ContractType : ui8 {
    INVALID,
    ITEM_SHIPMENT,
};

// A contract is a binding agreement between two entities
class IContract {
    friend class ContractManager;
public:
    virtual ~IContract() = default;
protected:
    IContract() = delete;
    IContract(entt::entity entityA, entt::entity entityB, TimeSpanSec maxAllowedTime = std::numeric_limits<TimeSpanSec>::max());

    virtual void endContract(entt::registry& registry, ContractEndReason endReason);

    ContractManager* mContractManager = nullptr;
    TimeStampSec mBeginTime = 0.0;
    TimeSpanSec mMaxAllowedTime = 0.0;
    ContractID mID = INVALID_CONTRACT_ID;
    entt::entity mEntityA = INVALID_ENTITY;
    entt::entity mEntityB = INVALID_ENTITY;
    ContractType mType = ContractType::INVALID;
};
