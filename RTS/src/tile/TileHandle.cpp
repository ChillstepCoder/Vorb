#include "stdafx.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"
#include "tile/TileContainerRepository.h"

#include "world/World.h"
#include "world/IChunkGrid.h"

#include "world/Chunk.h"


TileRef::TileRef(TileContainer* container, TileIndex index) :
    container(container),
    index(index),
    tile(&container->mTiles[index]) {
    container->incRef();
}

TileRef::TileRef(TileHandle handle) :
    container(const_cast<TileContainer*>(handle.container)), // FUCK YOU I DO WHAT I WANT
    index(handle.tileIndex),
    tile(&container->mTiles[handle.tileIndex]) {
    container->incRef();
}

TileRef::TileRef() {

}

void TileRef::acquire(TileHandle handle) {
    if (handle.container) {
        ASSERT_GAME_THREAD();
        assert(!container);
        container = const_cast<TileContainer*>(handle.container); // FUCK YOU I DO WHAT I WANT;
        index = handle.tileIndex;
        tile = &container->mTiles[index];
        container->incRef();
    }
}

void TileRef::acquire(TileContainer* newContainer, TileIndex newIndex) {
    if (newContainer) {
        ASSERT_GAME_THREAD();
        assert(!container);
        container = newContainer;
        index = newIndex;
        tile = &container->mTiles[newIndex];
        container->incRef();
    }
}

void TileRef::release()
{
    if (container) {
        container->decRef();
        container = nullptr;
    }
}

i32v2 TileRef::getWorldPos2D() const {
    return container->getTileSpatialGrid().getTileBaseWorldPos2D(index);
}

i32v3 TileRef::getWorldPos3D() const {
    return container->getTileSpatialGrid().getTileWorldPos3D(index, tile->getGroundZOffset());
}

TileHandle::TileHandle(const TileContainer* container, TileIndex tileIndex) :
    container(container),
    tileIndex(tileIndex) {

}

i32v2 TileHandle::getWorldPos2D() const {
    return container->getTileSpatialGrid().getTileBaseWorldPos2D(tileIndex);
}

i32v3 TileHandle::getWorldPos3D() const {
    return container->getTileSpatialGrid().getTileWorldPos3D(tileIndex, getTile().getGroundZOffset());
}

ui32v3 TileHandle::getContainerOffset() const {
    return container->getTileSpatialGrid().getTileXYZOffset(tileIndex);
}

inline const Tile& TileHandle::getTile() const {
    assert(container);
    return container->getTileAt(tileIndex);
}

LiteTileHandle TileHandle::toLiteTileHandle() const {
    return LiteTileHandle(container->getId(), tileIndex);
}

ChunkID TileHandle::getChunkIDAtPos() const
{
    if (!container) {
        return INVALID_CHUNK_ID;
    }
    const World& world = container->getWorld();
    return world.getChunkGrid().getChunkIDFromWorldPos(getWorldPos2D());
}

World& TileHandle::getWorld() const { return container->getWorld(); }

TileContainer* LiteTileHandle::getTileContainer(World& world) const {
    return world.getTileContainerRepository().getTileContainer(containerId);
}

TileContainer* LiteTileHandle::tryGetTileContainer(World& world) const {
    return world.getTileContainerRepository().tryGetTileContainer(containerId);
}

TileHandle LiteTileHandle::toTileHandle(World& world) const {
    return TileHandle(tryGetTileContainer(world), index);
}

i32v3 LiteTileHandle::getWorldPosition(World& world) const {
    assert(isValid());
    TileContainer* container = getTileContainer(world);
    return container->getTileSpatialGrid().getTileWorldPos3D(index, container->getTileAt(index).getGroundZOffset());
}

f32v3 LiteTileHandle::getWorldPositionCenter(World& world) const {
    assert(isValid());
    TileContainer* container = getTileContainer(world);
    return container->getTileCenterWorldPosition(index);
}
