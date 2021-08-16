#include "stdafx.h"
#include "ItemRenderer.h"

#include "ResourceManager.h"
#include "MaterialRenderer.h"

BatchedItemRenderer::BatchedItemRenderer(ResourceManager& resourceManager, MaterialRenderer& materialRenderer)
    : mResourceManager(resourceManager)
    , mMaterialRenderer(materialRenderer) {

}

BatchID BatchedItemRenderer::beginNewBatch(ui32 reserveQuadCount /*= 0*/) {
    assert(mInProgressBatch == INVALID_BATCH_ID);

    BatchID id = mItemMeshes.size();
    mItemMeshes.emplace_back();
    mInProgressBatch = id;
    mInProgressBatchData.clear();
    if (reserveQuadCount) {
        mInProgressBatchData.reserve(reserveQuadCount);
    }
    return id;
}

void BatchedItemRenderer::beginBatch(BatchID batchID, ui32 reserveQuadCount /*= 0*/) {
    assert(mInProgressBatch == INVALID_BATCH_ID);
    assert(batchID < mItemMeshes.size());

    mItemMeshes[batchID].init();
    mInProgressBatch = batchID;
    mInProgressBatchData.clear();
    if (reserveQuadCount) {
        mInProgressBatchData.reserve(reserveQuadCount);
    }
}

void BatchedItemRenderer::addItemStackToBatch(ui32v2& pos, ItemStack& itemStack) {
    assert(mInProgressBatch != INVALID_BATCH_ID);

}

void BatchedItemRenderer::finishBatch(BatchID batchID) {
    assert(mInProgressBatch != INVALID_BATCH_ID);
    // TODO: Fix texture param
    mItemMeshes[mInProgressBatch].setData(mInProgressBatchData.data(), mInProgressBatchData.size(), 0 /* TODO FIX */, QuadMeshDrawMode::STATIC);
    mInProgressBatch = INVALID_BATCH_ID;
}

void BatchedItemRenderer::deleteBatch(BatchID batchID) {
    assert(mInProgressBatch == INVALID_BATCH_ID);
    assert(batchID < mItemMeshes.size());
    mItemMeshes[batchID].destroy();
}

void BatchedItemRenderer::renderBatches()
{

}

void BatchedItemRenderer::renderItemStackOnGroundSingle(ui32v2& pos, ItemStack& itemStack)
{

}
