#pragma once

#include "item/ItemStack.h"

struct Recipe {
    Recipe() = default;
    ~Recipe() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(Recipe);

    std::unique_ptr<ItemStack[]> mItems;
    ui32 mItemCount;
    // RecipeFlags?
};
