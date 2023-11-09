#include "stdafx.h"
#include "BuildingBlueprint.h"
#include "Building.h"

#include "resources/TileRepository.h"

#include "math/Random.h"

BuildingBlueprint::BuildingBlueprint(
    World& world,
    const BuildingDef& desc,
    float sizeAlpha,
    Cartesian entrySide,
    ui32v2 dims,
    const i32v3& worldPosRoot,
    entt::entity ownerEntity,
    BuildingBlueprintFlags flags
) :
    world(&world), desc(&desc), sizeAlpha(sizeAlpha), entrySide(entrySide), mOwnerEntity(ownerEntity), flags(flags) {
    TileRepository& tileRepo = TileRepository::get();
    // We will reinitialize later with the proper Z dimensions
    mTileSpatialGrid.init(worldPosRoot, i32v3(dims.x, dims.y, 1), 3);
    // TODO: Different per building
    tileIDs[e_cast(BlueprintTileType::NONE)] = TILE_ID_NONE;
    tileIDs[e_cast(BlueprintTileType::FLOOR)] = tileRepo.getTileID(CStrToken("bricks_01"));
    tileIDs[e_cast(BlueprintTileType::DOOR)] = tileRepo.getTileID(CStrToken("wd_door_goth"));
    tileIDs[e_cast(BlueprintTileType::WALL)] = tileRepo.getTileID(CStrToken("wd_wall_goth"));
    tileIDs[e_cast(BlueprintTileType::WINDOW)] = tileRepo.getTileID(CStrToken("wd_wind_goth"));
    tileIDs[e_cast(BlueprintTileType::STAIRS)] = tileRepo.getTileID(CStrToken("stairs_wd"));
    tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)] = tileRepo.getTileID(CStrToken("stairs_wd_f"));
    tileIDs[e_cast(BlueprintTileType::AIR)] = TILE_ID_NONE;
    static_assert(e_cast(BlueprintTileType::TYPES) == 8);

}

BuildingBlueprint::~BuildingBlueprint() = default;

TileHandle BuildingBlueprint::getTileHandle(TileIndex tileIndex) const {
    ASSERT_GAME_THREAD();
    return TileHandle(building->getTileContainer(), tileIndex);
}

PlaceTileBlueprintItemsHandlePtr BuildingBlueprint::reserveTileToPlaceItems(ItemID itemId, ui16 maxItemCount) {
    assert(maxItemCount);
    auto&& it = tilesNeedingItems.find(itemId);
    if (it == tilesNeedingItems.end()) {
        return nullptr;
    }

    // Reverse iterate since we prefer pulling from the end
    for (auto&& rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
        const TileIndex tileIndex = *rit;
        const BlueprintTileItemDataHandle& itemDataHandle = tileItemDataHandles[tileIndex];
        for (ui32 i = 0; i < itemDataHandle.mItemDataCountRequired; ++i) {
            BlueprintTileItemData& itemData = tileItemData[itemDataHandle.mItemDataOffset + i];
            if (itemData.mItemId == itemId) {
                if (itemData.mPromisedQuantity < itemData.mMissingQuantity) {
                    PlaceTileBlueprintItemsHandlePtr handle = std::make_unique<PlaceTileBlueprintItemsHandle>();
                    handle->mTileIndex = tileIndex;
                    handle->mItemId = itemId;
                    handle->mPromisedItemCount = std::min(maxItemCount, (ui16)(itemData.mMissingQuantity - itemData.mPromisedQuantity));
                    handle->mBlueprint = this;
                    itemData.mPromisedQuantity += handle->mPromisedItemCount;
                    ++refCount;
                    return std::move(handle);
                }
                break;
            }
        }
    }
    return nullptr;
}

void BuildingBlueprint::cancelReserveTileToPlaceItems(PlaceTileBlueprintItemsHandle& handle) {
    assert(handle.mBlueprint == this);
    handle.mBlueprint = nullptr;
    --refCount;

    if (handle.mPromisedItemCount == 0) {
        return;
    }

    auto&& it = tilesNeedingItems.find(handle.mItemId);
    assert(it != tilesNeedingItems.end());

    const BlueprintTileItemDataHandle& itemDataHandle = tileItemDataHandles[handle.mTileIndex];
    for (ui32 i = 0; i < itemDataHandle.mItemDataCountRequired; ++i) {
        BlueprintTileItemData& itemData = tileItemData[itemDataHandle.mItemDataOffset + i];
        if (itemData.mItemId == handle.mItemId) {
            assert(itemData.mPromisedQuantity >= handle.mPromisedItemCount);
            itemData.mPromisedQuantity -= handle.mPromisedItemCount;
            handle.mPromisedItemCount = 0;
            return;
        }
    }
    assert(false); // NOT FOUND
}

BuildTileBlueprintHandlePtr BuildingBlueprint::reserveTileToBuild(entt::entity builderEntity, const f32v3& entityPosition) {
    if (tilesReadyToBuild.empty()) {
        return nullptr;
    }

    // Get closest via linear check
    size_t closest = UINT32_MAX;
    f32 closestDistanceSq = FLT_MAX;
    const i32v3& rootPos = mTileSpatialGrid.getWorldPos3D();
    const i32v3& dims = mTileSpatialGrid.getDims();
    const i32 layerSize = dims.x * dims.y;
    for (size_t i = 0; i < tilesReadyToBuild.size(); ++i) {
        const f32v3 tilePosition = f32v3(rootPos.x + (i % dims.x), rootPos.y + ((i % layerSize) / dims.x), rootPos.z + (i / layerSize) * mTileSpatialGrid.getFloorHeight());
        f32 distSq = glm::length2(entityPosition - tilePosition);
        if (distSq < closestDistanceSq) {
            closest = i;
        }
    }
    const TileIndex buildTileIndex = tilesReadyToBuild[closest];
    tilesReadyToBuild[closest] = tilesReadyToBuild.back();
    tilesReadyToBuild.pop_back();

    assert(tileBuildData[buildTileIndex].mReservedBy == INVALID_ENTITY);
    tileBuildData[buildTileIndex].mReservedBy = builderEntity;

    BuildTileBlueprintHandlePtr buildHandle = std::make_unique<BuildTileBlueprintHandle>();
    buildHandle->mBlueprint = this;
    buildHandle->mTileIndex = buildTileIndex;
    return std::move(buildHandle);
}

void BuildingBlueprint::endTileToBuild(BuildTileBlueprintHandle& handle) {
    assert(handle.mBlueprint == this);
    handle.mBlueprint = nullptr;
    --refCount;

    BlueprintTileBuildData& buildData = tileBuildData[handle.mTileIndex];
    buildData.mReservedBy = INVALID_ENTITY;
    if (buildData.mProgress >= 1.0f) {
        const BlueprintTileType type = tiles[handle.mTileIndex];
        assert(type != BlueprintTileType::NONE);
        TileContainer& container = *(building->getTileContainer());
        container.setOwnedTile(handle.mTileIndex);
        // TODO: STAIRS
        if (type != BlueprintTileType::STAIRS) {
            const TileID tileId = tileIDs[e_cast(type)];
            if (tileId != TILE_ID_NONE) {
                const TileDef& data = TileRepository::get().getLoadedOrUnloadedAsset(tileId);
                container.setOwnedTile(handle.mTileIndex);
                container.setTileGroundZPosition(handle.mTileIndex, 0.0f);
                container.setTileLayer(handle.mTileIndex, data);
            }
        }
        ++tilesBuilt;
    }
    else {
        tilesReadyToBuild.emplace_back(handle.mTileIndex);
    }
}

PlaceTileBlueprintItemsHandle::~PlaceTileBlueprintItemsHandle() {
    if (mBlueprint) {
        if (mPromisedItemCount) {
            mBlueprint->cancelReserveTileToPlaceItems(*this);
        }
        else {
            --mBlueprint->refCount;
        }
    }
}

void PlaceTileBlueprintItemsHandle::fulfillFromItemStack(ItemStack& stack) {
    assert(mBlueprint);

    BlueprintTileItemDataHandle& itemDataHandle = mBlueprint->tileItemDataHandles[mTileIndex];
    for (ui32 i = 0; i < itemDataHandle.mItemDataCountRequired; ++i) {
        BlueprintTileItemData& itemData = mBlueprint->tileItemData[itemDataHandle.mItemDataOffset + i];
        if (itemData.mItemId == mItemId) {
            const ui16 quantityToAdd = std::min((ui16)stack.quantity, itemData.mMissingQuantity);
            if (quantityToAdd == 0) {
                // Someone else filled this, so we can just cancel
                mPromisedItemCount = 0;
                return;
            }
            itemData.mMissingQuantity -= quantityToAdd;
            // Its possible that we lost some items on the way, or delivered too many, and thats OK
            const ui16 quantityToDecPromise = std::max(quantityToAdd, mPromisedItemCount);
            if (itemData.mPromisedQuantity >= quantityToDecPromise) {
                itemData.mPromisedQuantity -= quantityToDecPromise;
            }
            else {
                itemData.mPromisedQuantity = 0;
            }
            mPromisedItemCount = 0;
            stack.quantity -= quantityToAdd;
            // Check if we no longer require this item for this tile
            if (itemData.mMissingQuantity == 0) {
                ++itemDataHandle.mItemDataCountFinished;

                if (itemDataHandle.mItemDataCountFinished == itemDataHandle.mItemDataCountRequired) {
                    // Ready to build
                    mBlueprint->tilesReadyToBuild.emplace_back(mTileIndex);
                }

                auto&& it = mBlueprint->tilesNeedingItems.find(stack.id);
                assert(it != mBlueprint->tilesNeedingItems.end());
                // Remove tracking
                for (auto&& rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
                    const TileIndex tileIndex = *rit;
                    if (tileIndex == mTileIndex) {
                        *rit = it->second.back();
                        it->second.pop_back();
                        if (it->second.empty()) {
                            mBlueprint->tilesNeedingItems.erase(it);
                        }
                        return;
                    }
                }
            }
            return;
        }
    }
    assert(false); // Should be impossible?
}

BuildTileBlueprintHandle::~BuildTileBlueprintHandle() {
    if (mBlueprint) {
        mBlueprint->endTileToBuild(*this);
    }
}

bool BuildTileBlueprintHandle::tick(f32 buildProgressIncrease) {
    assert(mBlueprint);
    BlueprintTileBuildData& buildData = mBlueprint->tileBuildData[mTileIndex];
    buildData.mProgress += buildProgressIncrease;
    if (buildData.mProgress >= 1.0f) {
        mBlueprint->endTileToBuild(*this);
        return true;
    }
    return false;
}
