#pragma once

#include "world/Chunk.h"

DECL_VG(class SpriteBatch);

class ResourceManager;
class Camera2D;
class ChunkMesher;
class TextureAtlas;
class MaterialRenderer;
class World;
class Material;

class ChunkRenderer {
public:
	ChunkRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
	~ChunkRenderer();

	void renderWorld(const World& world, const Camera2D& camera);

	// Different rendering methods
	void RenderChunk(const Chunk& chunk, const Camera2D& camera);
	//void RenderChunkBaked(const Chunk& chunk, const Camera2D& camera, int LOD);

	// TODO: Deep LOD?
	void ReloadShaders();

    void InitPostLoad();
private:
	void UpdateMesh(const Chunk& chunk);

	std::unique_ptr<ChunkMesher> mMesher;
	ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
	const Material* standardMaterial = nullptr;
};

