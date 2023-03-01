#pragma once

constexpr ui16 MAX_ITEM_STACK_SIZE = UINT16_MAX;
constexpr ui16 MAX_ITEM_RESERVATION_SIZE = MAX_ITEM_STACK_SIZE;

// Maximum stack size is 65,535
struct ItemStack {
    ItemID id = INVALID_ITEM_ID;
    ui16 quantity = 0;

    bool isNull() const { return quantity == 0; }
};
static_assert(sizeof(ItemStack) == 4);

// Maximum stack size is 4,294,967,295 
struct ItemStackUnbounded {
    ItemID id = INVALID_ITEM_ID;
    ui32 quantity = 0;

    bool isNull() const { return quantity == 0; }
};
static_assert(sizeof(ItemStackUnbounded) == 8);