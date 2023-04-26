#include "stdafx.h"
#include "TileContainerRepository.h"

#include "tile/TileContainer.h"


TileContainerRepository::TileContainerRepository()
{
}

TileContainerRepository::~TileContainerRepository()
{
}

TileContainer* TileContainerRepository::getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, VarTileContainerOwner owner) {
    assert(IS_GAME_THREAD());
    std::unique_ptr<TileContainer> newContainer = std::make_unique<TileContainer>();
    TileContainer* rv = newContainer.get();

    // Ensure no collisions
    while (mTileContainers.find(mTileContainerIdGen) != mTileContainers.end()) {
        ++mTileContainerIdGen;
        if (mTileContainerIdGen > INT32_MAX) {
            mTileContainerIdGen = 0;
        }
    }

    newContainer->init(mTileContainerIdGen++, rootPos, dims, floorHeight, owner);
    // Clamp to int to prevent issues with PhysicsWorld storing these as signed integers
    if (mTileContainerIdGen > INT32_MAX) {
        mTileContainerIdGen = 0;
    }
    {
        std::lock_guard lock(mMutex);
        mTileContainers.insert(std::make_pair(newContainer->mId, rv));
    }
    return rv;
}

void TileContainerRepository::destroyTileContainer(TileContainer* container) {
    // TODO: Maybe just dont destroy this on the game thread
    assert(IS_GAME_THREAD() || IS_SHUTTING_DOWN);

    const TileContainerEvent destroyEvent{ container, {} };
    TileContainerRepository::dispatchDestroy(destroyEvent);
    container->dispatchDestroy(destroyEvent);
    {
        std::lock_guard lock(mMutex);
        mTileContainers.erase(container->mId);
    }
}

TileContainer* TileContainerRepository::getTileContainer(TileContainerID id) {
    assert(IS_GAME_THREAD());
    auto&& it = mTileContainers.find(id);
    assert(it != mTileContainers.end());
    return it->second;
}

TileContainer* TileContainerRepository::tryGetTileContainer(TileContainerID id) {
    // Can be done on worker thread so requires lock
    std::lock_guard lock(mMutex);
    auto&& it = mTileContainers.find(id);
    if (it == mTileContainers.end()) {
        return nullptr;
    }
    return it->second;
}
