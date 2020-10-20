#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "World.h"
#include "ResourceManager.h"
#include "rendering/ChunkMesher.h"
#include "rendering/QuadMesh.h"
#include "rendering/TileVertex.h"
#include "rendering/ShaderLoader.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/RenderContext.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

#define ENABLE_DEBUG_RENDER 1
#if ENABLE_DEBUG_RENDER == 1
#include <Vorb/ui/InputDispatcher.h>
#endif

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
	mResourceManager(resourceManager),
	mMaterialRenderer(materialRenderer)
{
    mMesher = std::make_unique<ChunkMesher>(resourceManager.getTextureAtlas());
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::renderWorld(const World& world, const Camera2D& camera)
{
#if ENABLE_DEBUG_RENDER == 1
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_R)) {
        if (!s_wasTogglePressed) {
            s_wasTogglePressed = true;
            s_debugToggle = !s_debugToggle;
        }
    }
    else {
        s_wasTogglePressed = false;
    }
#endif

    ChunkID chunkId;
    const Chunk* chunk;
    while (world.enumVisibleChunks(camera, chunkId, &chunk)) {
        if (chunk && chunk->canRender()) {
            RenderChunk(*chunk, camera);
        }
    }
}

void ChunkRenderer::renderWorldShadows(const World& world, const Camera2D& camera)
{
    ChunkID chunkId;
    const Chunk* chunk;
    while (world.enumVisibleChunks(camera, chunkId, &chunk)) {
        if (chunk && chunk->canRender()) {
            RenderChunkShadows(*chunk, camera);
        }
    }
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
        PreciseTimer timer;
		UpdateMesh(chunk);
        std::cout << "Mesh updated in " << timer.stop() << " ms\n";
	}

	RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*renderData.mChunkMesh, *standardMaterial);
}

void ChunkRenderer::RenderChunkShadows(const Chunk& chunk, const Camera2D& camera)
{
    RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*chunk.mChunkRenderData.mChunkMesh, *shadowMaterial, vg::DepthState::FULL);
}

void ChunkRenderer::ReloadShaders() {
	// reload dirty via timestamp
	assert(false);
	//for (auto& material : mMaterials) {
		//material.dispose();
	//}
	InitPostLoad();
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	mMesher->createMesh(chunk);
}

void ChunkRenderer::InitPostLoad()
{
	standardMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    assert(standardMaterial);
    shadowMaterial = mResourceManager.getMaterialManager().getMaterial("shadow");
    assert(shadowMaterial);
}
