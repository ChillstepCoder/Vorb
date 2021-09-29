#pragma once

#include "QuadMesh.h"
#include "rendering/TileVertex.h"

class ResourceManager;
class MaterialRenderer;
class ItemRepository;
class ItemStack;

typedef ui32 BatchID;
#define INVALID_BATCH_ID UINT32_MAX

// Render items on ground, useful for stockpiles or dropped items
class BatchedItemRenderer
{
public:
    BatchedItemRenderer(ResourceManager& resourceManager, MaterialRenderer& materialRenderer);

    BatchID beginNewBatch(ui32 reserveQuadCount = 0);
    void beginBatch(BatchID batchID, ui32 reserveQuadCount = 0);
    void addItemStackToBatch(const ui32v2& pos, const ItemStack& itemStack);
    void finishBatch(BatchID batchID);
    void deleteBatch(BatchID batchID);
    void renderBatches();
    void renderItemStackOnGroundSingle(const ui32v2& pos, const ItemStack& itemStack);

private:
    ResourceManager& mResourceManager;
    MaterialRenderer& mMaterialRenderer;
    ItemRepository& mItemRepository;

    std::vector<QuadMesh> mItemMeshes;
    std::vector<TileVertex> mInProgressBatchData;
    std::vector<BatchID> mFreeBatchIDs;
    BatchID mInProgressBatch = INVALID_BATCH_ID; ///< Current batch being built
};

