#include "stdafx.h"
#include "CliWorld.h"

#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

#include "ecs/cli/CliEntityComponentSystem.h"

#include "physics/PhysicsWorld.h"

CliWorld::CliWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(chunkGrid, heightmapGrid)
{
    initClient(*this);
    mEcs = std::make_unique<CliEntityComponentSystem>(*this);
}

void CliWorld::tick(f32 elapsedSec) {
    assert(IS_GAME_THREAD());

    // Update services
    Services::Threadpool::ref().mainThreadUpdate();

    // Update any pending updates if pathfinding is idle
    // TODO: REMOVE
    /*for (auto&& cid : getActiveChunks()) {
        mChunkGrid->getChunk(cid).updateMainThread();
    }*/

    mPhysWorld->stepSimulation(elapsedSec);

    // TODO: Figure out best order
    tickShared(elapsedSec);

    // TickClient always last as it updates render state
    tickClient(*this);
}

void CliWorld::onFrameBegin() {
    assert(IS_RENDER_THREAD());

}

void CliWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    assert(IS_RENDER_THREAD());
    // Physworld will handle internal interpolation and timestep itself
    //mPhysWorld->stepSimulation(elapsedSec);
}

void CliWorld::onWorldBegin(const f32v2& loadCenter) {
    onWorldBeginShared(loadCenter);
    onWorldBeginClient(*this);
}

void CliWorld::dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    cliDirtyGrassFromBrush(pos, brushRadius);
}

WorldNetMode CliWorld::getNetMode() {
    return WorldNetMode::Client;
}

WorldType CliWorld::getWorldType() {
    return WorldType::Game;
}
