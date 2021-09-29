#include "stdafx.h"
#include "ItemRenderer.h"
#include "item/ItemRepository.h"

#include "ResourceManager.h"
#include "MaterialRenderer.h"

BatchedItemRenderer::BatchedItemRenderer(ResourceManager& resourceManager, MaterialRenderer& materialRenderer)
    : mResourceManager(resourceManager)
    , mMaterialRenderer(materialRenderer)
    , mItemRepository(resourceManager.getItemRepository()) {

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

void BatchedItemRenderer::addItemStackToBatch(const ui32v2& pos, const ItemStack& itemStack) {
    assert(mInProgressBatch != INVALID_BATCH_ID);
    
    static constexpr float EPSILON = 0.005f;
    static constexpr f32 UV_EPSILON = 0.0001f;
    static constexpr f32 UV_EPSILON_2 = 0.0002f;

    mInProgressBatchData.resize(mInProgressBatchData.size() + 4);
    TileVertex* verts = &mInProgressBatchData.back() - 3;

    const Item& item = mItemRepository.getItem(itemStack.id);
    const SpriteData& spriteData = item.mSpriteData;
    const f32v4& uvs = spriteData.uvs;
    f32v4 adjustedUvs;
    adjustedUvs.x = uvs.x + UV_EPSILON;
    adjustedUvs.y = uvs.y + UV_EPSILON;
    adjustedUvs.z = uvs.z - UV_EPSILON_2;
    adjustedUvs.w = uvs.w - UV_EPSILON_2;

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor = topColor;

    // TODO: Variable Z
    f32v3 currentPos(pos.x, pos.y, 0.1f);

    { // Bottom Left
        TileVertex& vbl = verts[0];
        vbl.pos = currentPos;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = bottomColor;
        vbl.atlasPage = spriteData.atlasPage;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos = currentPos;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = bottomColor;
        vbr.atlasPage = spriteData.atlasPage;
        vbr.pos.x += spriteData.dimsMeters.x + EPSILON;
    }

    { // Top Left
        TileVertex& vtl = verts[2];
        vtl.pos = currentPos;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = topColor;
        vtl.atlasPage = spriteData.atlasPage;
        vtl.pos.y += spriteData.dimsMeters.y + EPSILON;
    }
    { // Top Right
        TileVertex& vtr = verts[3];
        vtr.pos = currentPos;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = topColor;
        vtr.atlasPage = spriteData.atlasPage;
        vtr.pos.x += spriteData.dimsMeters.x + EPSILON;
        vtr.pos.y += spriteData.dimsMeters.y + EPSILON;
    }
}

void BatchedItemRenderer::finishBatch(BatchID batchID) {
    assert(mInProgressBatch != INVALID_BATCH_ID);
    // TODO: Fix texture param
    mItemMeshes[mInProgressBatch].setData(mInProgressBatchData.data(), mInProgressBatchData.size(), QuadMeshDrawMode::STATIC);
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

void BatchedItemRenderer::renderItemStackOnGroundSingle(const ui32v2& pos, const ItemStack& itemStack)
{

}
