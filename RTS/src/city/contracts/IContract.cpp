#include "stdafx.h"
#include "IContract.h"

#include "city/contracts/ContractManager.h"

// For time query
#include "gamethread/GameThread.h"

IContract::IContract(entt::entity entityA, entt::entity entityB, TimeSpanSec maxAllowedTime /*= std::numeric_limits<TimeSpanSec>::max()*/) :
    mBeginTime(getCurrentTimeStamp()),
    mMaxAllowedTime(maxAllowedTime),
    mEntityA(entityA),
    mEntityB(entityB)
{
    assert(mEntityA != INVALID_ENTITY);
    assert(mEntityB != INVALID_ENTITY);
}

void IContract::endContract(entt::registry& registry, ContractEndReason endReason) {
    switch (endReason) {
        case ContractEndReason::COMPLETED:
            mContractManager->endContract(registry, this);
            return;
        case ContractEndReason::ENTITY_DEATH:
        case ContractEndReason::ENTITY_UNWILLING:
        case ContractEndReason::ENTITY_UNABLE:
        case ContractEndReason::TIME_EXPIRE:
            assert(false);
        default:
            break;
    }
}
