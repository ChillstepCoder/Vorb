#pragma once

#include "rendering/TileVertex.h"
// TODO: Instead include events?
#include "util/ThreadSafeDirtySet.h"
#include "item/ItemStockpile.h"
#include "rendering/mesh/Mesh.h"

#include "rendering/MaterialShaderDef.h"

class ItemRepository;
struct ItemStack;
class MaterialShaderDef;
class ItemStockpile;
class ItemDef;
class Camera3D;
struct ItemStockpileRecord;

#define INVALID_BATCH_ID UINT32_MAX

// Render items on ground, useful for stockpiles or dropped items
class ItemRenderer
{
public:
    ItemRenderer();

    void updateStockpileBillboardMesh(const ItemStockpile& stockpile) const;
    void updateStockpileQuadMesh(const ItemStockpile& stockpile) const;
    void addItemStackToMesh(Mesh& mesh, const f32v3& pos, const ItemStack& itemStack) const;

    void render(const Camera3D& camera) const;

private:
    void initEventHandlers();
    void updateDirtyStockpileMeshes() const;

    void renderMesh(const ItemStockpile& stockpile, const Mesh& itemMesh, const Camera3D& camera) const;
    void addItemStackPlanks(const ItemStockpileRecord& record, const ItemDef& item, const ItemStockpile& stockpile, Mesh& mesh) const;

    mutable ThreadSafeDirtySet<const ItemStockpile*> mDirtyStockpiles;
    moodycamel::ConcurrentQueue<ItemStockpileID> mMeshesToDestroy;
    moodycamel::ConcurrentQueue<std::pair<const ItemStockpile*, std::unique_ptr<Mesh>> > mFinishedMeshes;

    // TODO: boost flatmap?
    std::map<ItemStockpileID, std::unique_ptr<Mesh>> mStaticMeshes;

    ItemStockpileListeners mItemStockpileListeners;
    AssetHandle<MaterialShaderDef> mItemBillboardMaterial;
    AssetHandle<MaterialShaderDef> mItemMeshMaterial;
};
