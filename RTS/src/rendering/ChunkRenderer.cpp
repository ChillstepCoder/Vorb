#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "ResourceManager.h"
#include "rendering/ChunkMesher.h"
#include "rendering/ChunkMesh.h"
#include "rendering/ChunkVertex.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>


#define USE_NEW_MESHER

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager) :
	mResourceManager(resourceManager) // TODO: Remove?
{
    mMesher = std::make_unique<ChunkMesher>(resourceManager.getTextureAtlas());
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
        PreciseTimer timer;
		UpdateMesh(chunk);
        std::cout << "Mesh updated in " << timer.stop() << " ms\n";
	}

#ifdef USE_NEW_MESHER
	renderData.mChunkMesh->draw(camera);
#else
    renderData.mBaseMesh->render(f32m4(1.0f), camera.getCameraMatrix(), &vg::SamplerState::POINT_CLAMP, &vg::DepthState::FULL);
#endif
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
#ifdef USE_NEW_MESHER
	mMesher->createMesh(chunk);
#else
	mMesher->updateSpritebatch(chunk);
#endif
}
