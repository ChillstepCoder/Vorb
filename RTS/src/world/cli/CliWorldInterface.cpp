#include "stdafx.h"
#include "CliWorldInterface.h"

#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "resources/ResourceManager.h"
#include "particles/ParticleSystemManager.h"
#include "weather/CloudManager.h"

#include "rendering/mesh/TerrainMeshManager.h"

CliWorldInterface::CliWorldInterface()
{
    mCloudManager = std::make_unique<CloudManager>();
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>();
}

CliWorldInterface::~CliWorldInterface()
{

}


void CliWorldInterface::tickClient()
{
    // Update weather
    mCloudManager->tick();

    // Update terrain
    mTerrainMeshManager->tick();

}

void CliWorldInterface::enumVisibleChunks(std::function<void(const Chunk& chunk)> func) const {
    // TODO: Might be smart to make a variant that doesnt need an std::function for faster iteration/calls since
    // we call this many times
    assert(false);
    /*for (auto&& chunk : mVisibleChunks) {
        func(*chunk);
    }*/
}

void CliWorldInterface::updateParticleSystems(const f32v2& playerPos) {
    // Update particles (TODO: Ecs?)
    // TODO: eww why is a resource updating?
    Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);
}


void CliWorldInterface::onWorldBeginClient() {
    mCloudManager->init();
}

void CliWorldInterface::cliDirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    mTerrainMeshManager->dirtyTerrainFromBrush(pos, brushRadius);
}
