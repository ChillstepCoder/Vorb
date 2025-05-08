#include "stdafx.h"
#include "CraftingRepository.h"

#include "item/ItemRepository.h"

#include <Vorb/io/IOManager.h>

// TODO: Move?
struct ItemStackDef {
    nString itemName;
    ui32 count;
};
SERIALIZABLE_SIMPLE(ItemStackDef,
    make_field(o.itemName, "item"sv),
    make_field(o.count, "count"sv)
)

struct CraftingRecipeDef {
    std::vector<ItemStackDef> inputs;
    ItemStackDef output;
    ItemStackDef byProduct;
    nString requiredWorkStation;
    bool requiresWorkBench = false;
    ui32 work = 1;
};
SERIALIZABLE_SIMPLE(CraftingRecipeDef, 
    make_field(o.inputs, "inputs"sv),
    make_field(o.output, "output"sv),
    make_field(o.byProduct, "byProduct"sv),
    make_field(o.requiredWorkStation, "required_workstation"sv),
    make_field(o.requiresWorkBench, "requires_workbench"sv),
    make_field(o.work, "work"sv)
)

CraftingRepository::CraftingRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {
    // Add the null item, no lookup
    mCraftingRecipes.emplace_back();
}

void CraftingRepository::loadRecipeFile(const vio::Path& filePath) {
    const ItemRepository& itemRepo = ItemRepository::get();

    CraftingRecipeDef def;
    // TODO: DELETE ME WHEN USING ASSET REPO BASE
    nString fileData;
    if (!mIoManager.readFileToString(filePath, fileData)) {
        panic("Asset repository failed to read file {}", filePath.getCString());
    }
    YmlSerializer::readFileData(fileData, def);

    CraftingRecipe& recipe = mCraftingRecipes.emplace_back();
    recipe.mId = (CraftingRecipeID)(mCraftingRecipes.size() - 1);
    // Inputs
    recipe.mNumInputs = (ui32)def.inputs.size();
    assert(recipe.mNumInputs < MAX_CRAFTING_RECIPE_INPUTS);
    for (size_t i = 0; i < def.inputs.size(); ++i) {
        const ItemStackDef& itemStackDef = def.inputs[i];
        assert(itemRepo.assetExists(StrToken(itemStackDef.itemName)));
        recipe.mInputItem[i].id = itemRepo.getAssetID(StrToken(itemStackDef.itemName));
        recipe.mInputItem[i].count = itemStackDef.count;
    }
    // Output
    assert(def.output.itemName.size());
    recipe.mOutputItem.id = itemRepo.getAssetID(StrToken(def.output.itemName));
    recipe.mOutputItem.count = def.output.count;

    // By product
    if (def.byProduct.itemName.size()) {
        recipe.mByProduct.id = itemRepo.getAssetID(StrToken(def.byProduct.itemName));
        recipe.mByProduct.count = def.byProduct.count;
    }

    if (def.requiredWorkStation.size()) {
        recipe.mRequiredWorkStation = itemRepo.getAssetID(StrToken(def.requiredWorkStation));
    }
    recipe.mRequiresWorkbench = def.requiresWorkBench;
    recipe.mWork = def.work;

    mCraftingRecipesFromName[filePath.getFileNameNoExtension()] = recipe.mId;
}

std::vector<CraftingRecipe*> CraftingRepository::getAllCraftingRecipesWithInputs(std::vector<ItemID> inputs)
{
    std::vector<CraftingRecipe*> recipes;
    for (auto&& recipe : mCraftingRecipes) {
        for (ui32 i = 0; i < recipe.mNumInputs; ++i) {
            for (auto&& matchItem : inputs) {
                if (recipe.mInputItem[i].id == matchItem) {
                    recipes.emplace_back(&recipe);
                    // Break out of both loops
                    i = recipe.mNumInputs;
                    break;
                }
            }
        }
    }
    return recipes;
}

std::vector<CraftingRecipe*> CraftingRepository::getAllCraftingRecipesWithOutputs(std::vector<ItemID> outputs, bool includeByProduct /*= true*/) {
    std::vector<CraftingRecipe*> recipes;
    for (auto&& recipe : mCraftingRecipes) {
        for (auto&& matchItem : outputs) {
            if (recipe.mOutputItem.id == matchItem) {
                recipes.emplace_back(&recipe);
                break;
            }
            if (includeByProduct && recipe.mByProduct.id == matchItem) {
                recipes.emplace_back(&recipe);
                break;
            }
        }
    }
    return recipes;
}
