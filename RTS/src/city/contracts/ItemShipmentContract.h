#pragma once

#include "IContract.h"

// TODO: Boost pool allocate
// Entity A = shipper
// Entity B = shippee
class ItemShipmentContract : public IContract {
public:
    ItemShipmentContract(entt::entity entityA, entt::entity entityB, TimeSpanSec maxAllowedTime = std::numeric_limits<TimeSpanSec>::max()) : IContract(entityA, entityB, maxAllowedTime) {}

    ui16 mQuantity = 0;
    ItemID mItemId;
    bool mShipping = false;

};