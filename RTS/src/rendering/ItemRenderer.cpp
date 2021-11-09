#include "stdafx.h"
#include "ItemRenderer.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"

#include "services/Services.h"

#include "ResourceManager.h"
#include "MaterialRenderer.h"
#include "MaterialManager.h"

ItemRenderer::ItemRenderer(ResourceManager& resourceManager, MaterialRenderer& materialRenderer)
    : mResourceManager(resourceManager)
    , mMaterialRenderer(materialRenderer)
    , mItemRepository(resourceManager.getItemRepository()) {

    mItemMeshMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    mItemBillboardMaterial = mResourceManager.getMaterialManager().getMaterial("billboard");

}

void ItemRenderer::updateStockpileBillboardMesh(const ItemStockpile& stockpile) const {
    ItemStockpileRenderData& renderData = stockpile.mRenderData;
    // TODO: Multithread?
    if (!renderData.mBillboardMesh) {
        renderData.mBillboardMesh = std::make_unique<BillboardMesh>();
    }
    BillboardMesh& mesh = *renderData.mBillboardMesh;

    mesh.reserveQuadCount(stockpile.mTotalItems); // TODO: This is potentially way out of wack depending on number of quads/billboards

    for (auto& it : stockpile.mItemContents) {
        ItemID itemID = it.first;
        const ItemStockpileRecord& record = it.second;

        ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
        const Item& item = itemRepo.getItem(itemID);
        // Only points are billboards
        if (item.mShape != ItemStorageShape::POINT) {
            continue;
        }

        const SpriteData& spriteData = item.mSpriteData;

        for (ui32 index : record.stackLocations) {
            const ItemStack& stack = stockpile.mStorage[index];
            // TODO: Z
            ui32v2 pos2d = stockpile.mAABB.pos + ui32v2(index / stockpile.mAABB.width, index % stockpile.mAABB.width);
            f32v3 pos = f32v3(pos2d.x, pos2d.y, stockpile.mZPos);
            for (ui32 i = 0; i < stack.quantity; ++i) {
                f32v3 billboardPos = pos;
                // TODO: Not just 5 by 5
                constexpr ui32 w = 5;
                constexpr float spacingRatio = 1.0f / w;
                billboardPos.x += (i % w) * spacingRatio;
                billboardPos.y += ((i % (w * w)) / w) * spacingRatio;
                billboardPos.z += (i / (w * w)) * spacingRatio;
                mesh.addQuad(billboardPos, spriteData.dimsMeters * spacingRatio, f32v2(0.0f), spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, false, 0u);
            }
        }
    }

    mesh.finishMesh(MeshDrawMode::DYNAMIC);

    renderData.mBillboardMeshDirty = false;
}

void ItemRenderer::updateStockpileQuadMesh(const ItemStockpile& stockpile) const {

    ItemStockpileRenderData& renderData = stockpile.mRenderData;
    // TODO: Multithread?
    if (!renderData.mQuadMesh) {
        renderData.mQuadMesh = std::make_unique<QuadMesh>();
    }
    QuadMesh& mesh = *renderData.mQuadMesh;

    mesh.reserveQuadCount(stockpile.mTotalItems); // TODO: This is potentially way out of wack depending on number of quads/billboards

    for (auto& it : stockpile.mItemContents) {
        ItemID itemID = it.first;
        const ItemStockpileRecord& record = it.second;

        ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
        const Item& item = itemRepo.getItem(itemID);
        // Only points are billboards
        switch (item.mShape) {
            case ItemStorageShape::POINT:
                continue; // These are billboards
            case ItemStorageShape::PLANK:
                addItemStackPlanks(record, item, stockpile, mesh);
                break;
            case ItemStorageShape::LOG:
                break;
            case ItemStorageShape::INGOT:
                break;
            default:
                break;
        }
        static_assert(enum_cast(ItemStorageShape::COUNT) == 4, "Update for new mesh type");
    }

    mesh.finishMesh(MeshDrawMode::DYNAMIC);

    renderData.mBillboardMeshDirty = false;
}

void ItemRenderer::addItemStackToMesh(BillboardMesh& mesh, const f32v3& pos, const ItemStack& itemStack) const
{
    const Item& item = mItemRepository.getItem(itemStack.id);
    const SpriteData& spriteData = item.mSpriteData;
    const f32v4& uvs = spriteData.uvs;
    mesh.addQuad(pos, spriteData.dimsMeters, f32v2(0.0f), spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP, 0u);
}

void ItemRenderer::renderStockpile(const ItemStockpile& stockpile) const
{
    ItemStockpileRenderData& renderData = stockpile.mRenderData;

    // TODO: Multithreaded?
    if (renderData.mBillboardMeshDirty) {
        updateStockpileBillboardMesh(stockpile);
    }

    if (renderData.mQuadMeshDirty) {
        updateStockpileQuadMesh(stockpile);
    }

    if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
        renderMesh(*renderData.mBillboardMesh);
    }
    if (renderData.mQuadMesh && renderData.mQuadMesh->isValid()) {
        renderMesh(*renderData.mQuadMesh);
    }
}

void ItemRenderer::renderMesh(const BillboardMesh& itemMesh) const {
    mMaterialRenderer.renderMesh(itemMesh, *mItemBillboardMaterial);
}

void ItemRenderer::renderMesh(const QuadMesh& itemMesh) const {
    mMaterialRenderer.renderMesh(itemMesh, *mItemMeshMaterial);
}

void ItemRenderer::addItemStackPlanks(const ItemStockpileRecord& record, const Item& item, const ItemStockpile& stockpile, QuadMesh& mesh) const {

    const SpriteData& spriteData = item.mSpriteData;

    const ui32v3& stackDims = item.mStackDims;
    const ui32 stackLayer = stackDims.x * stackDims.y;
    f32v3 spacingRatio = f32v3(1.0f / stackDims.x, 1.0f / stackDims.y, 1.0f / stackDims.z);

    for (ui32 index : record.stackLocations) {
        const ItemStack& stack = stockpile.mStorage[index];
        // TODO: Z
        ui32v2 pos2d = stockpile.mAABB.pos + ui32v2(index % stockpile.mAABB.width, index / stockpile.mAABB.width);
        f32v3 pos = f32v3(pos2d.x, pos2d.y, stockpile.mZPos);
        for (ui32 i = 0; i < stack.quantity; ++i) {
            f32v3 boxPos = pos;
            // TODO: Not just 5 by 5
            constexpr ui32 w = 5;
            boxPos.x += (i % stackDims.x) * spacingRatio.x;
            boxPos.y += ((i % stackLayer) / stackDims.x) * spacingRatio.y;
            boxPos.z += (i / stackLayer) * spacingRatio.z;
            // TODO: Bottom
            // TODO: Cull edges, merging
            for (int i = enum_cast(QuadFacing::LEFT); i <= enum_cast(QuadFacing::TOP); ++i) {
                const f32v2& axis = QUAD_FACING_AXIS[i];
                const f32v2 dims(spacingRatio[axis.x], spacingRatio[axis.y]);
                mesh.addAxisAlignedQuad(boxPos + OBJECT_QUAD_FACING_GEOMETRY_OFFSETS[i] * spacingRatio, dims, f32v2(0.0f, 0.0f), axis, spriteData.atlasPage, spriteData.uvs, COLOR_WHITE, false);
            }
        }
    }

}
