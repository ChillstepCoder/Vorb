#include "stdafx.h"
#include "tile/TileHandle.h"

#include "world/Chunk.h"


TileRef::TileRef(TileContainer* container, TileIndex index) :
    container(container),
    index(index),
    tile(&container->mTiles[index]) {
    container->incRef();
}

TileRef::TileRef(TileHandle handle) :
    container(const_cast<TileContainer*>(handle.container)), // FUCK YOU I DO WHAT I WANT
    index(handle.index),
    tile(&container->mTiles[handle.index]) {
    container->incRef();
}

TileRef::TileRef() {

}

void TileRef::acquire(TileHandle handle) {
    assert(IS_MAIN_THREAD());
    assert(!container);
    container = const_cast<TileContainer*>(handle.container); // FUCK YOU I DO WHAT I WANT;
    index = handle.index;
    tile = &container->mTiles[index];
    container->incRef();
}

void TileRef::acquire(TileContainer* newContainer, TileIndex newIndex) {
    assert(IS_MAIN_THREAD());
    assert(!container);
    container = newContainer;
    index = newIndex;
    tile = &container->mTiles[newIndex];
    container->incRef();
}

void TileRef::release()
{
    if (container) {
        container->decRef();
        container = nullptr;
    }
}

TileHandle::TileHandle(const TileContainer* container, TileIndex index) :
    container(container),
    index(index),
    tile(&container->mTiles[index]) {

}

ui32v2 TileHandle::getWorldPos2D() const {
    return container->getWorldPos2D() + container->getTileXYOffset(index);
}

ui32v3 TileHandle::getContainerOffset() const {
    return container->getTileXYZOffset(index);
}
