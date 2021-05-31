#pragma once

DECL_VIO(class IOManager);
class ItemRepository;

#include "crafting/CraftingConst.h"
#include "item/Item.h"

struct CraftingRecipe {
    CraftingRecipeID mId = INVALID_CRAFTING_RECIPE_ID; // Array index into mCraftingRecipes
    ItemStack mInputItem[MAX_CRAFTING_RECIPE_INPUTS];
    ItemStack mOutputItem;
    ItemStack mByProduct;
    bool mRequiresWorkbench;
    ItemID mRequiredWorkStation;
    ui32 mNumInputs = 0;
    ui32 mWork = 1;
};

class CraftingRepository
{
public:
    CraftingRepository(vio::IOManager& ioManager);

    void loadRecipeFile(const ItemRepository& itemRepo, const vio::Path& filePath);

    std::vector<CraftingRecipe*> getAllCraftingRecipesWithInputs(std::vector<ItemID> inputs);
    std::vector<CraftingRecipe*> getAllCraftingRecipesWithOutputs(std::vector<ItemID> outputs, bool includeByProduct = true);

private:
    vio::IOManager& mIoManager;

    std::vector<CraftingRecipe> mCraftingRecipes;
    std::map<nString, CraftingRecipeID> mCraftingRecipesFromName;
};

