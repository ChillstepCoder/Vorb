#include "stdafx.h"
#include "CliWorldInterface.h"

#include "camera/Camera3D.h"
#include "world/Chunk.h"
#include "world/IWorld.h"
#include "world/IChunkGrid.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "ecs/IEntityComponentSystem.h"
#include "resources/ResourceManager.h"
#include "particles/ParticleSystemManager.h"
#include "weather/CloudMeshManager.h"
#include "options/DebugOptions.h"

#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/renderdata/WorldRenderDataManager.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/GrassMeshManager.h"

#include "rendering/renderstate/RenderStateManager.h"
#include "rendering/RenderThreadTasks.h"

void onCharacterModelConstruct(entt::registry& registry, entt::entity entity) {
    ModelID modelId = registry.get<CharacterModelComponent>(entity).modelId;
    LOG_DEBUG("Added model ID {} for entity {}", modelId, e_cast(entity));
    RenderThreadTasks::getInstance().addCharacterModel(entity, registry.get<CharacterModelComponent>(entity).modelId);
}

void onCharacterModelDestroy(entt::registry& registry, entt::entity entity) {
    LOG_DEBUG("Destroying model for entity {}", e_cast(entity));
    RenderThreadTasks::getInstance().removeCharacterModel(entity);
}

CliWorldInterface::CliWorldInterface() {

}

CliWorldInterface::~CliWorldInterface() {
    mCliWorld->getECS().mRegistry.on_construct<CharacterModelComponent>().disconnect<&onCharacterModelConstruct>();
    mCliWorld->getECS().mRegistry.on_destroy<CharacterModelComponent>().disconnect<&onCharacterModelDestroy>();
}

void CliWorldInterface::initClient(IWorld& world) {

}

void CliWorldInterface::tickClient(IWorld& world) {

    RenderContext::getInstance().tickGameThread(world);

    updateRenderState(world);
}

void CliWorldInterface::updateParticleSystems(const f32v2& playerPos) {
    // Update particles (TODO: Ecs?)
    // TODO: eww why is a resource updating?
    Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);
}

void CliWorldInterface::onWorldBeginClient(IWorld& world) {
    mCliWorld = &world;
    // When character models are added, we should let the render thread know
    mCliWorld->getECS().mRegistry.on_construct<CharacterModelComponent>().connect<&onCharacterModelConstruct>();
    mCliWorld->getECS().mRegistry.on_destroy<CharacterModelComponent>().connect<&onCharacterModelDestroy>();

    // Register for rendering
    RenderStateManager::getInstance().setActiveWorld(&world);
}

void CliWorldInterface::cliDirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    PROFILE_FUNCTION();
    assert(false); // REAL GRASS UPDATES
    //mGrassMeshManager->dirtyGrassFromBrush(pos, brushRadius);
}

void CliWorldInterface::updateRenderState(IWorld& world) {

    if (!RenderStateManager::getInstance().isActiveWorld(&world)) {
        return;
    }

    // Cache things we need to update so we can keep the update section small as possible
    entt::entity playerEntity = world.mEcs->getLocalPlayer();
    f32v3 cameraEntityPos = f32v3(0.0f);
    if (playerEntity != INVALID_ENTITY) {
        cameraEntityPos = world.mEcs->mRegistry.get<PhysicsComponent>(playerEntity).getInterpolatedPosition();
    }

    // Acquire render state
    RenderState& renderState = RenderStateManager::getInstance().getRenderStateForUpdate();
    renderState.mWorld = &world;
    renderState.mWorldLoadCenter = world.getLoadCenter();
    renderState.mCameraOwningEntityPos = cameraEntityPos;

    updateEntitiesRenderState(world, renderState);
    updateDebugRenderState(world, renderState);

    // Release render state
    RenderStateManager::getInstance().finishUpdating();
}

void CliWorldInterface::updateEntitiesRenderState(IWorld& world, RenderState& renderState) {

    IEntityComponentSystem& ecs = *world.mEcs;
    entt::registry& registry = ecs.mRegistry;
    auto view = registry.view<PhysicsComponent, CharacterControlComponent, CharacterModelComponent>();

    renderState.mCharacters.clear();

    // Construct fresh list of all entities
    for (auto entity : view) {
        PhysicsComponent& physCmp = view.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = view.get<CharacterControlComponent>(entity);
        if (!controlCmp.mFlags.isBitSet(CharacterControlComponentFlags::HIDE_MODEL)) {
            renderState.mCharacters.emplace_back(CharacterRenderState{ entity, physCmp.getPosition(), controlCmp.mControllerAngle, controlCmp.mMode });
        }
    };
}

void CliWorldInterface::updateDebugRenderState(IWorld& world, RenderState& renderState) {
    PROFILE_FUNCTION();

    renderState.mDebugQuads.clear();
    // Terrain debug rendering
    if (sDebugOptions.mDebugTerrainLod) {
        WorldRenderDataManager* manager = RenderContext::getInstance().tryGetRenderDataManagerForWorld(world);
        if (manager) {
            for (auto&& terrainQuadtree : manager->getTerrainMeshManager().getTerrainQuadtrees()) {
                terrainQuadtree.getDebugQuads(renderState.mDebugQuads);
            }
        }
    }

    // Grass debug rendering
    if (sDebugOptions.mDebugGrassLod) {
        WorldRenderDataManager* manager = RenderContext::getInstance().tryGetRenderDataManagerForWorld(world);
        if (manager) {
            for (auto&& it : manager->getGrassMeshManager().getGrassQuadtrees()) {
                if (it.second) {
                    it.second->getDebugQuads(renderState.mDebugQuads);
                }
            }
        }
    }

    // Chunk debug rendering
    if (sDebugOptions.mChunkBoundaries) {
        const IChunkGrid& chunkGrid = world.getChunkGrid();
        const auto& loadingChunks = chunkGrid.getLoadingChunks();
        const auto& activeChunks = chunkGrid.getActiveChunks();
        const auto& destroyingChunks = chunkGrid.getDestroyingChunks();
        renderState.mDebugChunks.resize(loadingChunks.size() + activeChunks.size() + destroyingChunks.size());

        int i = 0;

        // Add loading chunks
        for (auto&& cid : loadingChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::LOADING, (ui8)chunk.getRefCount(), flags };
        }

        // Add active chunks
        for (auto&& cid : activeChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::ACTIVE, (ui8)chunk.getRefCount(), flags };
        }

        // Add destroying chunks
        for (auto&& cid : destroyingChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::DESTROYING, (ui8)chunk.getRefCount(), flags };
        }
    }
    else {
        renderState.mDebugChunks.clear();
    }
}
