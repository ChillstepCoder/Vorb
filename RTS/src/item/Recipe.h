#pragma once

#include "item/ItemStack.h"

constexpr i32 MAX_RECIPE_ITEM_STACK_SIZE = UINT8_MAX;
constexpr i32 MAX_ITEMS_IN_RECIPE = 4;

struct Recipe {
    Recipe() = default;
    ~Recipe() = default;
    ItemID itemIds[MAX_ITEMS_IN_RECIPE] = { INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID };
    ui8 quantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    i8 numItems = 0;
    // RecipeFlags?
};
static_assert(sizeof(Recipe) == 14, "Keep small");

// Used for anything, usually tiles
// Represents a recipe that can be filled and completed, such as for tiles in building blueprints
// Tile ID stored separately
class FillableRecipe {
public:
    FillableRecipe() = default;
    FillableRecipe(const Recipe& recipe) {
        memcpy(itemIds, recipe.itemIds, sizeof(ItemID) * MAX_ITEMS_IN_RECIPE);
        memcpy(requiredQuantities, recipe.quantities, sizeof(ui8) * MAX_ITEMS_IN_RECIPE);
        static_assert(MAX_RECIPE_ITEM_STACK_SIZE == UINT8_MAX);
        numItems = recipe.numItems;
    }

    bool isFullyFilled() const {
        return memcmp(requiredQuantities, providedQuantities, sizeof(ui8) * MAX_ITEMS_IN_RECIPE) == 0;
    }

    ui8 getRemainingQuantityForItem(ItemID itemId) const {
        for (i32 i = 0; i < numItems; ++i) {
            if (itemIds[i] == itemId) {
                return requiredQuantities[i] - providedQuantities[i];
            }
        }
        return 0;
    }
    ui8 getRemainingQuantityAtIndex(int index) const {
        return requiredQuantities[index] - providedQuantities[index];
    }

    // Returns amount remaining from the provided quantity
    i32 fillItemAndReturnRemainder(ItemID itemId, i32 quantity) {
        for (i32 i = 0; i < numItems; ++i) {
            if (itemIds[i] == itemId) {
                const i32 remaining = i32(requiredQuantities[i] - providedQuantities[i]);
                const i32 toFill = glm::min(remaining, quantity);
                providedQuantities[i] += (ui8)toFill;
                return quantity - toFill;
            }
        }
        return quantity;
    }

    std::span<const ItemID> getRequiredItems() const {
        return std::span<const ItemID>(itemIds, numItems);
    }
    std::span<const ui8> getRequiredQuantities() const {
        return std::span<const ui8>(requiredQuantities, numItems);
    }
    std::span<const ui8> getProvidedQuantities() const {
        return std::span<const ui8>(providedQuantities, numItems);
    }
    i8 getNumItems() const { return numItems; }
    // Optional, useful for tile targets
    bool isConstructed() const { return constructed; }
    // Optional, useful for tile targets
    void setConstructed(bool value) { constructed = value; }

private:

    ItemID itemIds[MAX_ITEMS_IN_RECIPE] = { INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID,INVALID_ITEM_ID };
    ui8 requiredQuantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    ui8 providedQuantities[MAX_ITEMS_IN_RECIPE] = { 0,0,0,0 };
    i8 numItems = 0;
    bool constructed = false; // Optional, taking advantage of extra space
};
static_assert(sizeof(FillableRecipe) == 18,  "Keep small");
