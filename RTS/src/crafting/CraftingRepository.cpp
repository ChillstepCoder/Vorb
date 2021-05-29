#include "stdafx.h"
#include "CraftingRepository.h"

#include "item/ItemRepository.h"

#include <Vorb/io/IOManager.h>

// TODO: Move?
struct ItemStackDef {
    nString itemName;
    ui32 count;
};
KEG_TYPE_DEF_SAME_NAME(ItemStackDef, kt) {
    kt.addValue("item", keg::Value::basic(offsetof(ItemStackDef, itemName), keg::BasicType::STRING));
    kt.addValue("count", keg::Value::basic(offsetof(ItemStackDef, count), keg::BasicType::UI32));
}

struct CraftingRecipeDef {
    Array<ItemStackDef> inputs;
    ItemStackDef output;
    ItemStackDef byProduct;
    nString requiredWorkStation;
    bool requiresWorkBench = false;
    ui32 work = 1;
};
KEG_TYPE_DEF_SAME_NAME(CraftingRecipeDef, kt) {
    kt.addValue("inputs", keg::Value::array(offsetof(CraftingRecipeDef, inputs), keg::Value::custom(0, "ItemStackDef", false)));
    kt.addValue("output", keg::Value::custom(offsetof(CraftingRecipeDef, output), "ItemStackDef", false));
    kt.addValue("byProduct", keg::Value::custom(offsetof(CraftingRecipeDef, byProduct), "ItemStackDef", false));
    kt.addValue("requiredWorkStation", keg::Value::basic(offsetof(CraftingRecipeDef, byProduct), keg::BasicType::STRING));
    kt.addValue("requiresWorkBench", keg::Value::basic(offsetof(CraftingRecipeDef, requiresWorkBench), keg::BasicType::BOOL));
    kt.addValue("work", keg::Value::basic(offsetof(CraftingRecipeDef, byProduct), keg::BasicType::UI32));
}

CraftingRepository::CraftingRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {
    // Add the null item, no lookup
    mCraftingRecipes.emplace_back();
}

void CraftingRepository::loadRecipeFile(const ItemRepository& itemRepo, const vio::Path& filePath) {
    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        CraftingRecipeDef def;
        keg::parse((ui8*)&def, value, readContext, &KEG_GLOBAL_TYPE(CraftingRecipeDef));

        CraftingRecipe& recipe = mCraftingRecipes.emplace_back();
        recipe.mId = mCraftingRecipes.size() - 1;
        // Inputs
        recipe.mNumInputs = def.inputs.size();
        assert(recipe.mNumInputs < MAX_CRAFTING_RECIPE_INPUTS);
        for (size_t i = 0; i < def.inputs.size(); ++i) {
            const ItemStackDef& itemStackDef = def.inputs[i];
            recipe.mInputItem[i].id = itemRepo.getItem(itemStackDef.itemName).getID();
            recipe.mInputItem[i].quantity = itemStackDef.count;
        }
        // Output
        recipe.mOutputItem.id = itemRepo.getItem(def.byProduct.itemName).getID();
        recipe.mOutputItem.quantity = def.byProduct.count;

        // By product
        recipe.mByProduct.id = itemRepo.getItem(def.byProduct.itemName).getID();
        recipe.mByProduct.quantity = def.byProduct.count;

        recipe.mRequiredWorkStation = itemRepo.getItem(def.requiredWorkStation).getID();
        recipe.mRequiresWorkbench = def.requiresWorkBench;
        recipe.mWork = def.work;

        // TODO: Check for mod conflicts
        mCraftingRecipesFromName[key] = recipe.mId;

    }))) {
        // Do nothing on success
    }
    else {
        // Failure case
        pError("Failed to parse item file " + filePath.getString());
    }
}

std::vector<CraftingRecipe*> CraftingRepository::getAllCraftingRecipesWithInputs(std::vector<ItemID> inputs)
{

}

std::vector<CraftingRecipe*> CraftingRepository::getAllCraftingRecipesWithOutputs(std::vector<ItemID> inputs, bool includeByProduct /*= true*/)
{

}
