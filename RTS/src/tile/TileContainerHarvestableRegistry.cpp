#include "stdafx.h"
#include "TileContainerHarvestableRegistry.h"

#include "tile/TileContainer.h"
#include "tile/TileContainerEvents.h"
#include "resources/TileRepository.h"

#include "debugging/DebugRenderer.h"

TileContainerHarvestableRegistry::TileContainerHarvestableRegistry()
{

}

TileContainerHarvestableRegistry::~TileContainerHarvestableRegistry()
{

}

void TileContainerHarvestableRegistry::init(const TileContainer& owner) {
    mOwner = &owner;
    const i32v3& containerDims = owner.getTileSpatialGrid().getDims();
    // Round up with / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH
    for (int i = 0; i < 3; ++i) {
        // TODO: see BitArray::resize for more efficient method
        if (containerDims[i] % TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH == 0) {
            mRegistriesDims[i] = containerDims[i] / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH;
        }
        else {
            mRegistriesDims[i] = containerDims[i] / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH + 1;
        }
        if (containerDims[i] % TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH == 0) {
            mRegistriesDims[i] = containerDims[i] / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH;
        }
    }

    mRegistryCount = mRegistriesDims.x * mRegistriesDims.y * mRegistriesDims.z;
    assert(mRegistryCount);

    mRegistries = std::unique_ptr<HarvestableSubchunkRegistry[]>(new HarvestableSubchunkRegistry[mRegistryCount]);
    mHarvestables = std::unique_ptr<TileHarvestable[]>(new TileHarvestable[mOwner->getNumTiles()]);
    for (size_t i = 0; i < mOwner->getNumTiles(); ++i) {
        mHarvestables[i] = TileHarvestable::NONE;
    }

    for (int i = 0; i < (int)TileHarvestable::COUNT; ++i) {
        mTotalHarvestables[i] = 0;
    }
}

void TileContainerHarvestableRegistry::destroy() {
    mRegistries.reset();
    mHarvestables.reset();
    mOwner = nullptr;
}

void TileContainerHarvestableRegistry::refreshFromOwner() {
    PROFILE_FUNCTION();
    assert(mOwner);
    for (int i = 0; i < (int)TileHarvestable::COUNT; ++i) {
        mTotalHarvestables[i] = 0;
    }
    for (ui32 i = 0; i < mRegistryCount; ++i) {
        mRegistries[i].mHarvestablePositions.clear();
        for (int j = 0; j < (int)TileHarvestable::COUNT; ++j) {
            mRegistries[i].mTotalHarvestables[j] = 0;
        }
    }

    const i32v3& containerDims = mOwner->getTileSpatialGrid().getDims();
    const std::vector<Tile>& tiles = mOwner->getTiles();

    // Cache all harvestable data
    TileIndex tileIndex = 0;
    for (ui32 z = 0; z < containerDims.z; ++z) {
        const ui32 rz = z / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH;
        const ui32 rzoffset = rz * mRegistriesDims.x * mRegistriesDims.y;
        for (ui32 y = 0; y < containerDims.y; ++y) {
            const ui32 ry = y / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH;
            const ui32 ryoffset = ry * mRegistriesDims.x;
            for (ui32 x = 0; x < containerDims.x; ++x, ++tileIndex) {
                const Tile& tile = tiles[tileIndex];
                TileID mainId = tile.getMainID();
                static_assert(TILE_LAYER_COUNT == 2, "If harvestables can exist on more than one level we need this to be a loop");
                if (isTileNone(mainId)) {
                    mHarvestables[tileIndex] = TileHarvestable::NONE;
                } else {
                    const TileHarvestable harvestable = TileRepository::getTileData(tile.getMainID()).harvestable;
                    mHarvestables[tileIndex] = harvestable;
                    if (harvestable != TileHarvestable::NONE) {
                        ++mTotalHarvestables[e_cast(harvestable)];
                        const ui32 rx = x / TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH;
                        const ui32 registryIndex = rzoffset + ryoffset + rx;
                        HarvestableSubchunkRegistry& subchunkRegistry = mRegistries[registryIndex];
                        ++subchunkRegistry.mTotalHarvestables[e_cast(harvestable)];
                        subchunkRegistry.mHarvestablePositions.insert(std::make_pair(tileIndex, harvestable));
                    }
                }
            }
        }
    }
}

void TileContainerHarvestableRegistry::debugDraw() const {
    // NOTE: NOT THREAD SAFE
    assert(IS_RENDER_THREAD());
    if (!mOwner) {
        return;
    }

    constexpr ui32 DEBUG_DURATION = 500;
    constexpr f32 ALPHA = 1.0f;
    const color4 colors[e_cast(TileHarvestable::COUNT)] = {
        color4(0.8f, 0.25f, 0.25f, ALPHA), // WOOD
        color4(0.4f, 0.4f, 0.4f, ALPHA) // STONE
    };
    static_assert(e_cast(TileHarvestable::COUNT) == 2);

    for (ui32 i = 0; i < mRegistryCount; ++i) {
        for (auto&& it : mRegistries[i].mHarvestablePositions) {
            f32v3 centerPos = mOwner->getTileCenterWorldPosition(it.first);
            DebugRenderer::drawWireQuad(centerPos - f32v3(0.5f, 0.5f, 0.0f), f32v2(1.0f), colors[(int)it.second], DEBUG_DURATION);
        }
    }

}

void TileContainerHarvestableRegistry::onTileLayerChanged(TileContainerEditEvent& evnt) {
    assert(evnt.type == TileContainerEditEventType::ChangeLayer);
    for (ui32 i = 0; i < evnt.editCount; ++i) {
        TileContainerEditLayerEventData& editData = evnt.changeLayerArray[i];
        // Only main layer matters
        if ((int)editData.layer == TILE_LAYER_MAIN) {
            TileHarvestable prevHarvestable = TileHarvestable::NONE;
            TileHarvestable newHarvestable = TileHarvestable::NONE;

            if (!isTileNone(editData.prevId)) {
                prevHarvestable = TileRepository::getTileData(editData.prevId).harvestable;
            }
            if (!isTileNone(editData.newId)) {
                newHarvestable = TileRepository::getTileData(editData.newId).harvestable;
            }

            if (newHarvestable != prevHarvestable) {
                const SubchunkIndex registryIndex = mOwner->getSubchunkIndexFromTileIndex(editData.tileIndex);
                HarvestableSubchunkRegistry& subchunkRegistry = mRegistries[registryIndex];
                if (prevHarvestable != TileHarvestable::NONE) {
                    --mTotalHarvestables[e_cast(prevHarvestable)];
                    --subchunkRegistry.mTotalHarvestables[e_cast(prevHarvestable)];
                    if (newHarvestable != TileHarvestable::NONE) {
                        // Replacing harvestable with a new harvestable
                        ++mTotalHarvestables[e_cast(newHarvestable)];
                        ++subchunkRegistry.mTotalHarvestables[e_cast(newHarvestable)];
                        subchunkRegistry.mHarvestablePositions[editData.tileIndex] = newHarvestable;
                    }
                    else {
                        subchunkRegistry.mHarvestablePositions.erase(editData.tileIndex);
                    }
                } else {
                    // Simply adding a new harvestable where there was none before
                    ++mTotalHarvestables[e_cast(newHarvestable)];
                    ++subchunkRegistry.mTotalHarvestables[e_cast(newHarvestable)];
                    subchunkRegistry.mHarvestablePositions[editData.tileIndex] = newHarvestable;
                }
            }
        }
    }
}
