#include "stdafx.h"
#include "BuildingBlueprint.h"

#include "resources/TileRepository.h"

BuildingBlueprint::BuildingBlueprint(
    const BuildingDef& desc,
    float sizeAlpha,
    Cartesian entrySide,
    ui32v2 dims,
    ui32v2 bottomLeftWorldPos,
    entt::entity ownerEntity,
    BuildingBlueprintFlags flags
) :
    desc(desc), sizeAlpha(sizeAlpha), entrySide(entrySide), aabb(bottomLeftWorldPos.x, bottomLeftWorldPos.y, dims.x, dims.y), mOwnerEntity(ownerEntity), flags(flags) {

    // TODO: Different per building
    tileIDs[e_cast(BlueprintTileType::NONE)] = TILE_ID_NONE;
    tileIDs[e_cast(BlueprintTileType::FLOOR)] = TileRepository::getTile(StrToken("bricks", 1));
    tileIDs[e_cast(BlueprintTileType::DOOR)] = TileRepository::getTile(StrToken("door"));
    tileIDs[e_cast(BlueprintTileType::WALL)] = TileRepository::getTile(StrToken("wd_wall_goth"));
    tileIDs[e_cast(BlueprintTileType::STAIRS)] = TileRepository::getTile(StrToken("stairs_wd"));
    tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)] = TileRepository::getTile(StrToken("stairs_wd_f"));
    tileIDs[e_cast(BlueprintTileType::AIR)] = TILE_ID_NONE;

    static_assert(e_cast(BlueprintTileType::TYPES) == 7);

}

PlaceTileBlueprintItemsHandlePtr BuildingBlueprint::reserveTileToPlaceItems(ItemID itemId, ui16 maxItemCount) {
    auto&& it = tilesNeedingItems.find(itemId);
    if (it == tilesNeedingItems.end()) {
        return nullptr;
    }

    for (auto&& h : it->second) {
        for (ui32 i = 0; i < h.mItemDataCount; ++i) {
            BlueprintTileItemData& itemData = tileItemData[h.mItemDataOffset + i];
            if (itemData.mItemId == itemId) {
                if (itemData.mPromisedQuantity < itemData.mMissingQuantity) {
                    PlaceTileBlueprintItemsHandlePtr handle = std::make_unique<PlaceTileBlueprintItemsHandle>();
                    handle->mTileHandle = h;
                    handle->mItemId = itemId;
                    handle->mItemCount = std::min(maxItemCount, (ui16)(itemData.mMissingQuantity - itemData.mPromisedQuantity));
                    handle->mBlueprint = this;
                    return std::move(handle);
                }
                break;
            }
        }
    }
    return nullptr;
}

void BuildingBlueprint::cancelReserveTileToPlaceItems(PlaceTileBlueprintItemsHandle& handle) {
    if (handle.mItemCount == 0) {
        return;
    }

    auto&& it = tilesNeedingItems.find(handle.mItemId);
    assert(it != tilesNeedingItems.end());

    const BlueprintTileHandle& tileHandle = handle.mTileHandle;
    for (ui32 i = 0; i < tileHandle.mItemDataCount; ++i) {
        BlueprintTileItemData& itemData = tileItemData[tileHandle.mItemDataOffset + i];
        if (itemData.mItemId == handle.mItemId) {
            itemData.mPromisedQuantity -= handle.mItemCount;
            handle.mItemCount = 0;
            return;
        }
    }
    assert(false); // NOT FOUND
}
