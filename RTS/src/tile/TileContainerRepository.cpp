#include "stdafx.h"
#include "TileContainerRepository.h"

#include "world/World.h"
#include "world/LocalChunk.h"

#include "tile/TileContainer.h"

TileContainerRepository::TileContainerRepository(World& world) : mWorld(world) {};
TileContainerRepository::~TileContainerRepository() = default;

TileContainer* TileContainerRepository::allocateChunkContainer(i32v2 rootPos, LocalChunk* owner) {
    owner->mTileContainer = allocateNewTileContainer(i32v3(rootPos.x, rootPos.y, 0), i32v3(CHUNK_WIDTH, CHUNK_WIDTH, 1), 1, owner);
    return owner->mTileContainer;
}

TileContainer* TileContainerRepository::createNewEmptyBuildingContainer(i32AABB3 tileAABB, ui32 floorHeight, Building* owner) {
    TileContainer* container = allocateNewTileContainer(tileAABB.pos, tileAABB.dims, floorHeight, owner);
    container->allocateData();
    return container;
}

void TileContainerRepository::destroyTileContainer(TileContainer* container) {
    // TODO: Maybe just dont destroy this on the game thread
    assert(IS_GAME_THREAD() || mWorld.isShuttingDown());
    assert(container->getRefCount() <= 1 || mWorld.isShuttingDown());

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

TileContainer* TileContainerRepository::allocateNewTileContainer(i32v3 rootPos, i32v3 dims, ui32 floorHeight, VarTileContainerOwner owner) {
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
