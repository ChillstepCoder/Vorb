#include "stdafx.h"
#include "ItemRenderer.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"

#include "resources/ResourceManager.h"
#include "MaterialRenderer.h"
#include "MaterialShaderManager.h"
#include "rendering/mesh/Mesh.h"
#include "camera/Camera3D.h"

ItemRenderer::ItemRenderer() {

    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mItemMeshMaterial = materialManager.getMaterialShader("standard_tile");
  //  mItemBillboardMaterial = materialManager.getMaterial("billboard");

}

void ItemRenderer::updateStockpileBillboardMesh(const ItemStockpile& stockpile) const {
    //ItemStockpileRenderData& renderData = stockpile.mRenderData;
    //// TODO: Multithread?
    //if (!renderData.mBillboardMesh) {
    //    renderData.mBillboardMesh = std::make_unique<TBOBillboardMesh>();
    //}
    //TBOBillboardMesh& mesh = *renderData.mBillboardMesh;

    //mesh.reserveQuadCount(stockpile.mTotalItems); // TODO: This is potentially way out of wack depending on number of quads/billboards

    //for (auto& it : stockpile.mItemContents) {
    //    ItemID itemID = it.first;
    //    const ItemStockpileRecord& record = it.second;

    //    ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
    //    const Item& item = itemRepo.getItem(itemID);
    //    // Only points are billboards
    //    if (item.mShape != ItemStorageShape::POINT) {
    //        continue;
    //    }

    //    const SubTexture& texture = item.mTexture;

    //    for (ui32 index : record.stackLocations) {
    //        const ItemStack& stack = stockpile.mStorage[index].stack;
    //        // TODO: Z
    //        ui32v2 pos2d = ui32v2(index / stockpile.mAABB.width, index % stockpile.mAABB.width);
    //        f32v3 pos = f32v3(pos2d.x, pos2d.y, stockpile.mZPos);
    //        for (ui32 i = 0; i < stack.quantity; ++i) {
    //            f32v3 billboardPos = pos;
    //            // TODO: Not just 5 by 5
    //            constexpr ui32 w = 5;
    //            constexpr float spacingRatio = 1.0f / w;
    //            billboardPos.x += (i % w) * spacingRatio;
    //            billboardPos.y += ((i % (w * w)) / w) * spacingRatio;
    //            billboardPos.z += (i / (w * w)) * spacingRatio;
    //            mesh.addQuad(billboardPos, f32v2(0.3f) * spacingRatio, f32v2(0.0f), 0, texture.mUvRect, COLOR_WHITE, false, 0u, 0u);
    //        }
    //    }
    //}

    //mesh.finishMesh(MeshDrawMode::DYNAMIC);

   // renderData.mBillboardMeshDirty = false;
}

void ItemRenderer::updateStockpileQuadMesh(const ItemStockpile& stockpile) const {
    assert(false);
    //ItemStockpileRenderData& renderData = stockpile.mRenderData;
    //// TODO: Multithread?
    //if (!renderData.mQuadMesh) {
    //    renderData.mQuadMesh = std::make_unique<QuadMesh>();
    //}
    //QuadMesh& mesh = *renderData.mQuadMesh;

    //mesh.reserveQuadCount(stockpile.mTotalItems); // TODO: This is potentially way out of wack depending on number of quads/billboards

    //for (auto& it : stockpile.mItemContents) {
    //    ItemID itemID = it.first;
    //    const ItemStockpileRecord& record = it.second;

    //    ItemRepository& itemRepo = Services::ResourceManager::ref().getItemRepository();
    //    const Item& item = itemRepo.getItem(itemID);
    //    // Only points are billboards
    //    switch (item.mShape) {
    //        case ItemStorageShape::POINT:
    //            continue; // These are billboards
    //        case ItemStorageShape::PLANK:
    //            addItemStackPlanks(record, item, stockpile, mesh);
    //            break;
    //        case ItemStorageShape::LOG:
    //            break;
    //        case ItemStorageShape::INGOT:
    //            break;
    //        default:
    //            break;
    //    }
    //    static_assert(e_cast(ItemStorageShape::COUNT) == 4, "Update for new mesh type");
    //}

    //mesh.finishMesh(MeshDrawMode::DYNAMIC);

    //renderData.mQuadMeshDirty = false;
}

void ItemRenderer::addItemStackToMesh(Mesh& mesh, const f32v3& pos, const ItemStack& itemStack) const
{
    const Item& item = Services::ResourceManager::ref().getItemRepository().getItem(itemStack.id);
    //mesh.addQuad(pos, f32v2(1.0f), f32v2(0.0f), 0, uvs, COLOR_WHITE, false, 0u, 0u);
}

void ItemRenderer::renderStockpile(const ItemStockpile& stockpile, const Camera3D& camera) const
{
    assert(false);
    //ItemStockpileRenderData& renderData = stockpile.mRenderData;

    //// TODO: Multithreaded?
    //if (renderData.mBillboardMeshDirty) {
    //    updateStockpileBillboardMesh(stockpile);
    //}

    //if (renderData.mQuadMeshDirty) {
    //    updateStockpileQuadMesh(stockpile);
    //}

    ////if (renderData.mBillboardMesh && renderData.mBillboardMesh->isValid()) {
    ////    renderMesh(stockpile, *renderData.mBillboardMesh, camera);
    ////}
    //if (renderData.mQuadMesh && renderData.mQuadMesh->isValid()) {
    //    renderMesh(stockpile, *renderData.mQuadMesh, camera);
    //}
}

void ItemRenderer::renderMesh(const ItemStockpile& stockpile, const Mesh& itemMesh, const Camera3D& camera) const {
    VGUniform offsetUniform = mItemBillboardMaterial->mProgram.getUniform("unOffset");
    const f32v3 stockpilePos(stockpile.mAABB.pos.x, stockpile.mAABB.pos.y, 0.0f);
    const f32v3 offset = stockpilePos - camera.getPosition();

    // TODO: Reduce swaps
    MaterialRenderer::bindMaterialForRender(*mItemBillboardMaterial, nullptr);
    glUniform3fv(offsetUniform, 1, &offset.x);
    itemMesh.draw();
}


void ItemRenderer::addItemStackPlanks(const ItemStockpileRecord& record, const Item& item, const ItemStockpile& stockpile, Mesh& mesh) const {
    assert(false);
    //const SubTexture& texture = item.mTexture;

    //const ui32v3& stackDims = item.mStackDims;
    //const ui32 stackLayer = stackDims.x * stackDims.y;
    //f32v3 spacingRatio = f32v3(1.0f / stackDims.x, 1.0f / stackDims.y, 1.0f / stackDims.z);

    //for (ui32 index : record.stackLocations) {
    //    const ItemStack& stack = stockpile.mStorage[index].stack;
    //    // TODO: Z
    //    ui32v2 pos2d = ui32v2(index % stockpile.mAABB.width, index / stockpile.mAABB.width);
    //    f32v3 pos = f32v3(pos2d.x, pos2d.y, stockpile.mZPos);
    //    for (ui32 i = 0; i < stack.quantity; ++i) {
    //        f32v3 boxPos = pos;
    //        // TODO: Not just 5 by 5
    //        constexpr ui32 w = 5;
    //        boxPos.x += (i % stackDims.x) * spacingRatio.x;
    //        boxPos.y += ((i % stackLayer) / stackDims.x) * spacingRatio.y;
    //        boxPos.z += (i / stackLayer) * spacingRatio.z;
    //        // TODO: Bottom
    //        // TODO: Cull edges, merging
    //        for (int j = e_cast(CubeFacing::LEFT); j <= e_cast(CubeFacing::TOP); ++j) {
    //            const f32v2& axis = CUBE_FACING_AXIS[j];
    //            const f32v2 dims(spacingRatio[axis.x], spacingRatio[axis.y]);
    //            mesh.addAxisAlignedQuad(boxPos + CUBE_FACING_GEOMETRY_OFFSETS[j] * spacingRatio, dims, f32v2(0.0f, 0.0f), (CubeFacing)j, 0, texture.mUvRect, COLOR_WHITE, false);
    //        }
    //    }
    //}

}
