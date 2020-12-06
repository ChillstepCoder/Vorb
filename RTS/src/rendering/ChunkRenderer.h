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

enum class ChunkRenderLOD {
    FULL_DETAIL,
    LOD_TEXTURE,
    COUNT
};

// TODO: IRendererBase?
class ChunkRenderer {
public:
	ChunkRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
	~ChunkRenderer();

    void renderWorld(const World& world, const Camera2D& camera, ChunkRenderLOD lod);
    void renderWorldShadows(const World& world, const Camera2D& camera);

	//void RenderChunkBaked(const Chunk& chunk, const Camera2D& camera, int LOD);

	// TODO: Deep LOD?
	void ReloadShaders();

    void InitPostLoad();
private:
    // Different rendering methods
    void RenderFullDetail(const Chunk& chunk, const Camera2D& camera);
    void RenderFullDetailShadows(const Chunk& chunk, const Camera2D& camera);
    void RenderLODTexture(const Chunk& chunk, const Camera2D& camera);

	void UpdateFullDetailMesh(const Chunk& chunk);
    void UpdateLODTexture(const Chunk& chunk);

	std::unique_ptr<ChunkMesher> mMesher;
	ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
    const Material* mStandardMaterial = nullptr;
    const Material* mShadowMaterial = nullptr;
    const Material* mLODMaterial = nullptr;
};

