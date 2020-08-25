#pragma once

#include "world/Chunk.h"

DECL_VG(class SpriteBatch);
DECL_VG(class TextureCache);

class Camera2D;
class TileSet;

class ChunkRenderer {
public:
	ChunkRenderer(vg::TextureCache& textureCache);
	~ChunkRenderer();

	// Different rendering methods
	void RenderChunk(const Chunk& chunk, const Camera2D& camera);
	//void RenderChunkBaked(const Chunk& chunk, const Camera2D& camera, int LOD);

	// TODO: Deep LOD?

	// TODO: 3D Generated???
	// void RenderChunkAs3D(const Chunk& chunk, const Camera3D& camera)

private:
	void UpdateMesh(const Chunk& chunk);

	std::unique_ptr<TileSet> mTileSet;
};

