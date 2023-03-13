#include "stdafx.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"

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
        assert(IS_GAME_THREAD());
        assert(!container);
        container = const_cast<TileContainer*>(handle.container); // FUCK YOU I DO WHAT I WANT;
        index = handle.tileIndex;
        tile = &container->mTiles[index];
        container->incRef();
    }
}

void TileRef::acquire(TileContainer* newContainer, TileIndex newIndex) {
    if (newContainer) {
        assert(IS_GAME_THREAD());
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

TileHandle::TileHandle(const TileContainer* container, TileIndex tileIndex) :
    container(container),
    tileIndex(tileIndex) {
    if (container) {
        tile = &container->mTiles[tileIndex];
    }
}

i32v2 TileHandle::getWorldPos2D() const {
    return container->getWorldPos2D() + i32v2(container->getTileXYOffset(tileIndex));
}

i32v3 TileHandle::getWorldPos3D() const {
    return container->getWorldPos3D() + i32v3(container->getTileXYZOffsetWithZScale(tileIndex));
}

ui32v3 TileHandle::getContainerOffset() const {
    return container->getTileXYZOffset(tileIndex);
}

LiteTileHandle TileHandle::toLiteTileHandle() const {
    return LiteTileHandle(container->getId(), tileIndex);
}

TileContainer* LiteTileHandle::getTileContainer() const {
    return TileContainerRepository::getTileContainer(containerId);
}

TileContainer* LiteTileHandle::tryGetTileContainer() const {
    return TileContainerRepository::tryGetTileContainer(containerId);
}

TileHandle LiteTileHandle::toTileHandle() const {
    return TileHandle(tryGetTileContainer(), index);
}

i32v3 LiteTileHandle::getWorldPosition() const
{
    TileContainer* container = getTileContainer();
    return container->getTileCenterWorldPosition(index);;
}
