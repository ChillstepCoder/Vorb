#include "stdafx.h"
#include "CliWorldInterface.h"

#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "world/IWorld.h"
#include "ecs/IEntityComponentSystem.h"
#include "resources/ResourceManager.h"
#include "particles/ParticleSystemManager.h"
#include "weather/CloudManager.h"

#include "rendering/mesh/TerrainMeshManager.h"

#include "rendering/renderstate/RenderStateManager.h"

CliWorldInterface::CliWorldInterface() {
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>();
}

CliWorldInterface::~CliWorldInterface() {

}


void CliWorldInterface::tickClient(IWorld& world) {
    // Update terrain
    mTerrainMeshManager->tick();

    updateRenderState(world);
}

void CliWorldInterface::updateParticleSystems(const f32v2& playerPos) {
    // Update particles (TODO: Ecs?)
    // TODO: eww why is a resource updating?
    Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);
}


void CliWorldInterface::onWorldBeginClient() {

}

void CliWorldInterface::cliDirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    mTerrainMeshManager->dirtyTerrainFromBrush(pos, brushRadius);
}

void CliWorldInterface::updateRenderState(IWorld& world) {
    // Cache things we need to update so we can keep the update section small as possible
    const f32v3 playerPos = world.mEcs->mRegistry.get<PhysicsComponent>(world.mEcs->getLocalPlayer()).getInterpolatedPosition();

    // Get render state
    RenderState& renderState = RenderStateManager::getInstance().getRenderStateForUpdate();
    renderState.mWorldLoadCenter = world.getLoadCenter();
    renderState.mCameraOwningEntityPos = playerPos;

    // Release the state
    RenderStateManager::getInstance().finishUpdating();
}
