#pragma once

#include "world/Chunk.h"


DECL_VG(class SpriteBatch);

class ResourceManager;
class Camera2D;
class ChunkMesher;

class ChunkRenderer {
public:
	ChunkRenderer(ResourceManager& resourceManager);
	~ChunkRenderer();

	// Different rendering methods
	void RenderChunk(const Chunk& chunk, const Camera2D& camera);
	//void RenderChunkBaked(const Chunk& chunk, const Camera2D& camera, int LOD);

	// TODO: Deep LOD?

private:
	void UpdateMesh(const Chunk& chunk);

	std::unique_ptr<ChunkMesher> mMesher;
	ResourceManager& mResourceManager;
};

