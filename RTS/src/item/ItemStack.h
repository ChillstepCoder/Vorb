#pragma once

struct ItemStack {
    ItemID id = INVALID_ITEM_ID;
    ui32 quantity = 0;

    bool isNull() const { return quantity == 0; }
};