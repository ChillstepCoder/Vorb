#include "stdafx.h"
#include "ChunkRenderer.h"
#include "TileSet.h"
#include "Camera2D.h"
#include "world/Chunk.h"
#include "ResourceManager.h"

#include <Vorb/graphics/SpriteBatch.h>

ChunkRenderer::ChunkRenderer(ResourceManager& resourceManager) :
	mResourceManager(resourceManager) // TODO: Remove?
{

}

ChunkRenderer::~ChunkRenderer() {
	
}

void ChunkRenderer::RenderChunk(const Chunk& chunk, const Camera2D& camera) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;

	if (renderData.mBaseDirty) {
		UpdateMesh(chunk);
	}

	renderData.mBaseMesh->render(f32m4(1.0f), camera.getCameraMatrix());
}

void ChunkRenderer::UpdateMesh(const Chunk& chunk) {
	// mutable render data
	ChunkRenderData& renderData = chunk.mChunkRenderData;
	const i32v2& pos = chunk.getChunkPos();
	const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH);

	if (!renderData.mBaseMesh) {
		renderData.mBaseMesh = std::make_unique<vg::SpriteBatch>(true, true);
	}

	renderData.mBaseMesh->begin();
	for (int y = 0; y < CHUNK_WIDTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			std::cout << TileIndex(x, y) << " ";

			//  TODO: More than just ground
			const Tile& tile = chunk.getTileAt(TileIndex(x, y));
			const SpriteData& spriteData = TileRepository::getTileData(tile.groundLayer).spriteData;
			if (spriteData.method == TileTextureMethod::SIMPLE) {
				renderData.mBaseMesh->draw(spriteData.texture, &spriteData.uvs, f32v2(x + offset.x, y + offset.y), f32v2(spriteData.dims), color4(1.0f, 1.0f, 1.0f));
			}
			else {
				
				
			}
		}
	}
	renderData.mBaseMesh->end();

	renderData.mBaseDirty = false;
}
