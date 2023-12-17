#include "stdafx.h"
#include "TileContainerRepository.h"

#include "world/World.h"
#include "world/Chunk.h"

#include "tile/TileContainer.h"
#include "tile/TileContainerLoader.h"

TileContainerRepository::TileContainerRepository(World& world) : mWorld(world) {
    mLoader = std::make_unique<TileContainerLoader>(mWorld);
}

TileContainerRepository::~TileContainerRepository()
{
}

TileContainer* TileContainerRepository::loadChunk(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, Chunk* owner) {
    owner->mTileContainer = allocateNewTileContainer(rootPos, dims, floorHeight, owner);
    mLoader->loadChunk(*owner->mTileContainer);
    return owner->mTileContainer;
}

TileContainer* TileContainerRepository::createNewEmptyBuildingContainer(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, Building* owner) {
    TileContainer* container = allocateNewTileContainer(rootPos, dims, floorHeight, owner);
    container->allocateData();
    return container;
}

void TileContainerRepository::destroyTileContainer(TileContainer* container) {
    // TODO: Maybe just dont destroy this on the game thread
    assert(IS_GAME_THREAD() || IS_SHUTTING_DOWN);
    assert(container->getRefCount() <= 1 || IS_SHUTTING_DOWN);

    const TileContainerEvent destroyEvent{ container, {} };
    TileContainerRepository::dispatchDestroy(destroyEvent);
    container->dispatchDestroy(destroyEvent);
    {
        std::lock_guard lock(mMutex);
        mTileContainers.erase(container->mId);
    }
}

TileContainer* TileContainerRepository::getTileContainer(TileContainerID id) {
    ASSERT_GAME_THREAD();
    auto&& it = mTileContainers.find(id);
    assert(it != mTileContainers.end());
    return it->second.get();
}

TileContainer* TileContainerRepository::tryGetTileContainer(TileContainerID id) {
    // Can be done on worker thread so requires lock
    std::lock_guard lock(mMutex);
    auto&& it = mTileContainers.find(id);
    if (it == mTileContainers.end()) {
        return nullptr;
    }
    return it->second.get();
}

TileContainer* TileContainerRepository::allocateNewTileContainer(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, VarTileContainerOwner owner) {
    ASSERT_GAME_THREAD();
    std::unique_ptr<TileContainer> newContainer = std::make_unique<TileContainer>(mWorld);
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
        mTileContainers.insert(std::make_pair(newContainer->mId, std::move(newContainer)));
    }
    return rv;
}
