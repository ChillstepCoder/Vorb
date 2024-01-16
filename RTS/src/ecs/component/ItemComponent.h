#pragma once

#include "item/ItemStack.h"

class ItemComponent {
public:
    ItemStack itemStack;
};
static_assert(sizeof(ItemComponent) == 12, "Keep small");
