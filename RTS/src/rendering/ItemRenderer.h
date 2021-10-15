#pragma once

#include "QuadMesh.h"
#include "rendering/TileVertex.h"

class ResourceManager;
class MaterialRenderer;
class ItemRepository;
struct ItemStack;
class Material;
class ItemStockpile;
class Item;
struct ItemStockpileRecord;

#define INVALID_BATCH_ID UINT32_MAX

// Render items on ground, useful for stockpiles or dropped items
class ItemRenderer
{
public:
    ItemRenderer(ResourceManager& resourceManager, MaterialRenderer& materialRenderer);

    void updateStockpileBillboardMesh(const ItemStockpile& stockpile) const;
    void updateStockpileQuadMesh(const ItemStockpile& stockpile) const;
    void addItemStackToMesh(BillboardMesh& mesh, const f32v3& pos, const ItemStack& itemStack) const;
    void renderStockpile(const ItemStockpile& stockpile) const;
    void renderMesh(const BillboardMesh& itemMesh) const;
    void renderMesh(const QuadMesh& itemMesh) const;

private:
    void addItemStackPlanks(const ItemStockpileRecord& record, const Item& item, const ItemStockpile& stockpile, QuadMesh& mesh) const;

    ResourceManager& mResourceManager;
    MaterialRenderer& mMaterialRenderer;
    ItemRepository& mItemRepository;

    const Material* mItemBillboardMaterial;
    const Material* mItemMeshMaterial;
};
