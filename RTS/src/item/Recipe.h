#pragma once

#include "item/ItemStack.h"

constexpr i32 MAX_RECIPE_ITEM_STACK_SIZE = UINT8_MAX;
// TODO: Can this be 3?
constexpr i32 MAX_ITEMS_IN_RECIPE = 4;

struct Recipe {
    Recipe() = default;
    ~Recipe() = default;
    ItemID itemIds[MAX_ITEMS_IN_RECIPE] = { INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID };
    ui8 quantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    ui8 numItems = 0;
    // RecipeFlags?
};
static_assert(sizeof(Recipe) == 14, "Keep small");

// Used for anything, usually tiles
// Represents a recipe that can be filled and completed, such as for tiles in building blueprints
// Tile ID stored seperately
class FillableRecipe {
public:
    FillableRecipe() = default;
    FillableRecipe(const Recipe& recipe) {
        memcpy(itemIds, recipe.itemIds, sizeof(ItemID) * MAX_ITEMS_IN_RECIPE);
        memcpy(requiredQuantities, recipe.quantities, sizeof(ui8) * MAX_ITEMS_IN_RECIPE);
        static_assert(MAX_RECIPE_ITEM_STACK_SIZE == UINT8_MAX);
        numItems = recipe.numItems;
    }

    ItemID itemIds[MAX_ITEMS_IN_RECIPE] = { INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID };
    ui8 requiredQuantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    ui8 providedQuantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    ui8 numItems = 0;
    bool isFullyFilled = false;
};
static_assert(sizeof(FillableRecipe) == 18,  "Keep small");
