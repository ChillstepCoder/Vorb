#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"

#include <Vorb/graphics/SpriteBatch.h>
// MOVE TO RESOURCE MANAGER
#include <Vorb/graphics/TextureCache.h>

ChunkRenderer::ChunkRenderer(vg::TextureCache& textureCache)
{
	// TODO: Better resource manager for tilesets. We shouldnt need to hard code dimensions here. Load from .meta file
	// Resource manager should build all resources up front
	mTileSet = std::make_unique<TileSet>(textureCache, "data/textures/tiles.png", i32v2(10, 16));
}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mDirty) {
		UpdateMesh(chunk);
	}

	renderData.mSb->render(f32m4(1.0f), camera.getCameraMatrix());
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;
	const i32v2& pos = chunk.getPos();
	const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH - 1);

	if (!renderData.mSb) {
		renderData.mSb = std::make_unique<vg::SpriteBatch>(true, true);
	}

	renderData.mSb->begin();
	for (int y = 0; y < CHUNK_WIDTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			mTileSet->renderTile(*renderData.mSb, (int)chunk.getTileAt(x, y), f32v2(x + offset.x, y + offset.y));
		}
	}
	renderData.mSb->end();

	renderData.mDirty = false;
}
