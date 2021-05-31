#pragma once
typedef ui32 CraftingRecipeID; // Allows us to serialize crafting recipes so mods work, or for lookups
#define INVALID_CRAFTING_RECIPE_ID UINT32_MAX

constexpr ui32 MAX_CRAFTING_RECIPE_INPUTS = 4;