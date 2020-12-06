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

void ChunkRenderer::renderWorld(const World& world, const Camera2D& camera, ChunkRenderLOD lod)
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

    switch (lod) {
        case ChunkRenderLOD::FULL_DETAIL: {
            while (world.enumVisibleChunks(camera, chunkId, &chunk)) {
                if (chunk && chunk->canRender()) {
                    RenderFullDetail(*chunk, camera);
                }
            }
            break;
        }
        case ChunkRenderLOD::LOD_TEXTURE: {
            while (world.enumVisibleChunks(camera, chunkId, &chunk)) {
                if (chunk && chunk->canRender()) {
                    RenderLODTexture(*chunk, camera);
                }
            }
            break;
        }
        default:
            assert(false);
    }
    static_assert((int)ChunkRenderLOD::COUNT == 2, "Update");

}

void ChunkRenderer::renderWorldShadows(const World& world, const Camera2D& camera)
{
    ChunkID chunkId;
    const Chunk* chunk;
    while (world.enumVisibleChunks(camera, chunkId, &chunk)) {
        if (chunk && chunk->canRender()) {
            RenderFullDetailShadows(*chunk, camera);
        }
    }
}

void ChunkRenderer::RenderFullDetail(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
        PreciseTimer timer;
		UpdateFullDetailMesh(chunk);
        std::cout << "Mesh updated in " << timer.stop() << " ms\n";
	}

	RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*renderData.mChunkMesh, *mStandardMaterial);
}

void ChunkRenderer::RenderFullDetailShadows(const Chunk& chunk, const Camera2D& camera)
{
    RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*chunk.mChunkRenderData.mChunkMesh, *mShadowMaterial, vg::DepthState::FULL);
}

void ChunkRenderer::RenderLODTexture(const Chunk& chunk, const Camera2D& camera) {
    // mutable render data
    ChunkRenderData& renderData = chunk.mChunkRenderData;

    if (renderData.mLODDirty) {
        PreciseTimer timer;
        UpdateLODTexture(chunk);
        std::cout << "LOD updated in " << timer.stop() << " ms\n";
    }

    const f32v2 worldPos = chunk.getWorldPos();
    const f32v4 rect(worldPos.x, worldPos.y, CHUNK_WIDTH, CHUNK_WIDTH);

    RenderContext::getInstance().getMaterialRenderer().renderMaterialToQuadWithTexture(*mLODMaterial, renderData.mLODTexture, rect);
}

void ChunkRenderer::ReloadShaders() {
	// reload dirty via timestamp
	assert(false);
	//for (auto& material : mMaterials) {
		//material.dispose();
	//}
	InitPostLoad();
}

void ChunkRenderer::UpdateFullDetailMesh(const Chunk& chunk) {
	mMesher->createFullDetailMesh(chunk);
}

void ChunkRenderer::UpdateLODTexture(const Chunk& chunk) {
    mMesher->createLODTexture(chunk);
}

void ChunkRenderer::InitPostLoad()
{
	mStandardMaterial = mResourceManager.getMaterialManager().getMaterial("standard_tile");
    assert(mStandardMaterial);
    mShadowMaterial = mResourceManager.getMaterialManager().getMaterial("shadow");
    assert(mShadowMaterial);
    mLODMaterial = mResourceManager.getMaterialManager().getMaterial("chunk_lod");
    assert(mLODMaterial);
}
