#include "stdafx.h"
#include "CliWorldInterface.h"

#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "resources/ResourceManager.h"
#include "particles/ParticleSystemManager.h"
#include "weather/CloudManager.h"

CliWorldInterface::CliWorldInterface()
{
    mCloudManager = std::make_unique<CloudManager>();
}

CliWorldInterface::~CliWorldInterface()
{

}


void CliWorldInterface::tickClient()
{

}

void CliWorldInterface::enumVisibleChunks(std::function<void(const Chunk& chunk)> func) const {
    // TODO: Might be smart to make a variant that doesnt need an std::function for faster iteration/calls since
    // we call this many times
    for (auto&& chunk : mVisibleChunks) {
        func(*chunk);
    }
}

void CliWorldInterface::updateChunkVisibility(const Camera3D& camera, const std::vector<Chunk*>& activeChunks) {
    mVisibleChunks.clear();
    for (Chunk* chunk : activeChunks) {
        // Determine visibility
        if (chunk->isDataReady()) {
            const f32v2& worldPos = chunk->getWorldPos();
            if (camera.sphereIsVisible(f32v3(worldPos.x + HALF_CHUNK_WIDTH, worldPos.y + HALF_CHUNK_WIDTH, 0.0f), CHUNK_DIAGONAL_RADIUS + 30.0f /*padding for camera pan fix :C WHY*/)) { // TODO: Broken + AABB Test?
                mVisibleChunks.push_back(chunk);
                chunk->getTileContainer()->getRenderData().mIsVisible = true;
            }
            else {
                chunk->getTileContainer()->getRenderData().mIsVisible = false;
            }
        }
    }
}

void CliWorldInterface::updateParticleSystems(const f32v2& playerPos) {
    // Update particles (TODO: Ecs?)
    // TODO: eww why is a resource updating?
    Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);
}

void CliWorldInterface::updateClouds() {
    // Update weather
    mCloudManager->update();
}

void CliWorldInterface::initPostResourcesLoadedClient() {
    mCloudManager->init();
}
