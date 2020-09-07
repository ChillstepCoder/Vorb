#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "ResourceManager.h"
#include "ChunkMesher.h"

#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/DepthState.h>

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager) :
	mResourceManager(resourceManager) // TODO: Remove?
{
    mMesher = std::make_unique<ChunkMesher>();
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

	renderData.mBaseMesh->render(f32m4(1.0f), camera.getCameraMatrix(), &vg::SamplerState::POINT_CLAMP, &vg::DepthState::FULL);
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	mMesher->updateSpritebatch(chunk);
}
