#include "stdafx.h"
#include "InventoryComponent.h"

constexpr f32 ENCUMBER_MULT = 2.0f;

InventoryComponent::InventoryComponent(f32 maxCarryWeight) :
    mMaxCarryWeight(maxCarryWeight) {

}

f32 InventoryComponent::getEncumbermentValue() const
{
    const f32 diff = mTotalCarryWeight - mMaxCarryWeight;
    if (diff > 0.0f) {
        return (diff / mMaxCarryWeight) * ENCUMBER_MULT;
    }
    return 0.0f;
}
