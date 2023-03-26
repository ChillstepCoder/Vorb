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
#include "weather/CloudManager.h"
#include "options/DebugOptions.h"

#include "rendering/ChunkGrassQuadtree.h"

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
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>();
    mGrassMeshManager = std::make_unique<GrassMeshManager>();
}

CliWorldInterface::~CliWorldInterface() {
    sWorld->getECS().mRegistry.on_construct<CharacterModelComponent>().disconnect<&onCharacterModelConstruct>();
    sWorld->getECS().mRegistry.on_destroy<CharacterModelComponent>().disconnect<&onCharacterModelDestroy>();
}

void CliWorldInterface::tickClient(IWorld& world) {

    mTerrainMeshManager->tick();
    mGrassMeshManager->tick();

    updateRenderState(world);
}

void CliWorldInterface::updateParticleSystems(const f32v2& playerPos) {
    // Update particles (TODO: Ecs?)
    // TODO: eww why is a resource updating?
    Services::ResourceManager::ref().getParticleSystemManager().update(playerPos);
}

void CliWorldInterface::onWorldBeginClient() {
    // When character models are added, we should let the render thread know
    sWorld->getECS().mRegistry.on_construct<CharacterModelComponent>().connect<&onCharacterModelConstruct>();
    sWorld->getECS().mRegistry.on_destroy<CharacterModelComponent>().connect<&onCharacterModelDestroy>();

    sWorld->getChunkGrid().addReadyListener([this](const Chunk& chunk) {
        mGrassMeshManager->addGrassForChunk(chunk);
    });
    sWorld->getChunkGrid().addDestroyListener([this](const Chunk& chunk) {
        mGrassMeshManager->removeGrassForChunk(chunk);
    });
}

void CliWorldInterface::cliDirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    PROFILE_FUNCTION();
    mTerrainMeshManager->dirtyTerrainFromBrush(pos, brushRadius);
    mGrassMeshManager->dirtyGrassFromBrush(pos, brushRadius);
}

void CliWorldInterface::cliDirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    PROFILE_FUNCTION();
    mGrassMeshManager->dirtyGrassFromBrush(pos, brushRadius);
}

void CliWorldInterface::updateRenderState(IWorld& world) {
    // Cache things we need to update so we can keep the update section small as possible
    const f32v3 playerPos = world.mEcs->mRegistry.get<PhysicsComponent>(world.mEcs->getLocalPlayer()).getInterpolatedPosition();

    // Acquire render state
    RenderState& renderState = RenderStateManager::getInstance().getRenderStateForUpdate();
    renderState.mWorldLoadCenter = world.getLoadCenter();
    renderState.mCameraOwningEntityPos = playerPos;

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
        for (auto&& terrainQuadtree : mTerrainMeshManager->getTerrainQuadtrees()) {
            terrainQuadtree.getDebugQuads(renderState.mDebugQuads);
        }
    }

    // Grass debug rendering
    if (sDebugOptions.mDebugGrassLod) {
        for (auto&& it : mGrassMeshManager->getGrassQuadtrees()) {
            if (it.second) {
                it.second->getDebugQuads(renderState.mDebugQuads);
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
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getState(), DebugChunkListIndex::LOADING, (ui8)chunk.getRefCount(), flags };
        }

        // Add active chunks
        for (auto&& cid : activeChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getState(), DebugChunkListIndex::ACTIVE, (ui8)chunk.getRefCount(), flags };
        }

        // Add destroying chunks
        for (auto&& cid : destroyingChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getState(), DebugChunkListIndex::DESTROYING, (ui8)chunk.getRefCount(), flags };
        }
    }
    else {
        renderState.mDebugChunks.clear();
    }
}
