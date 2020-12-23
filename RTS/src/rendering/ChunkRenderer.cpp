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
#include "services/Services.h"

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


    // TODO: REMOVE
    PreciseTimer timer;
    timer.start();
    int i = 0;

    world.enumVisibleChunks(camera, [&](const Chunk& chunk) {
        ++i;
        if (chunk.isFinished()) {
            Render(chunk, camera, lod);
        }
    });
    f64 total = timer.stop();
    std::cout << "ENUMERATE " << i << " " << total << std::endl;

}

void ChunkRenderer::renderWorldShadows(const World& world, const Camera2D& camera)
{
    world.enumVisibleChunks(camera, [&](const Chunk& chunk) {
        if (chunk.isFinished()) {
            RenderShadows(chunk, camera);
        }
    });
}

void ChunkRenderer::Render(const Chunk& chunk, const Camera2D& camera, ChunkRenderLOD lod) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mIsBuildingMesh) {
        if (lod == ChunkRenderLOD::FULL_DETAIL && renderData.mBaseDirty) {
            mMesher->createMeshAsync(chunk);
        }
        else if (lod == ChunkRenderLOD::LOD_TEXTURE && renderData.mLODDirty) {
            mMesher->createLODTextureAsync(chunk);
        }
    }

    if (lod == ChunkRenderLOD::FULL_DETAIL && renderData.mChunkMesh) {
        RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*renderData.mChunkMesh, *mStandardMaterial);
    }
    else if (renderData.mLODTexture) {
        const f32v2 worldPos = chunk.getWorldPos();
        const f32v4 rect(worldPos.x, worldPos.y, CHUNK_WIDTH, CHUNK_WIDTH);

        RenderContext::getInstance().getMaterialRenderer().renderMaterialToQuadWithTexture(*mLODMaterial, renderData.mLODTexture, rect);
    }
    static_assert((int)ChunkRenderLOD::COUNT == 2, "Update");
}

void ChunkRenderer::RenderShadows(const Chunk& chunk, const Camera2D& camera)
{
    QuadMesh* mesh = chunk.mChunkRenderData.mChunkMesh.get();
    if (mesh && mesh->isValid()) {
        RenderContext::getInstance().getMaterialRenderer().renderQuadMesh(*mesh, *mShadowMaterial, vg::DepthState::FULL);
    }
}

void ChunkRenderer::ReloadShaders() {
	// reload dirty via timestamp
	assert(false);
	//for (auto& material : mMaterials) {
		//material.dispose();
	//}
	InitPostLoad();
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
