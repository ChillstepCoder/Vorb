#pragma once

#include "item/ItemStack.h"

enum class StructureFlags : ui8 {
    SettlementStorage = BIT(0),
    InConstruction = BIT(1),
};

struct StructureComponent {
    ChunkID rootChunkID;
    i32AABB2 aabb;
    entt::entity settlementEntity = entt::null;
    StructureFlags flags = {};
};

struct StructureInventory {
    boost::container::flat_multimap<ItemID, ItemStack> containedItems;
    f32 totalItemWeight = 0.0f;
    f32 itemWeightCapacity = 0.0f;
};

struct StructureConstructionComponent {
    f32 progress = 0.0f;
};