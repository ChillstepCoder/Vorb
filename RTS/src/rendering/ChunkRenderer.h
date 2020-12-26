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

	// TODO: Deep LOD?
	void ReloadShaders();

    void InitPostLoad();
private:
    // Different rendering methods
    void UpdateMesh(const Chunk& chunk);
    void UpdateLODTexture(const Chunk& chunk);
    void RenderMeshOrLODTexture(const Chunk& chunk, const Camera2D& camera);
    void RenderLODTexture(const f32v2& worldPos, VGTexture texture, f32 width, const Camera2D& camera);
    void RenderLODTextureBindless(const f32v2& worldPos, VGTexture texture, f32 width, const Camera2D& camera, ui32 textureIndex);
    void RenderShadows(const Chunk& chunk, const Camera2D& camera);

	std::unique_ptr<ChunkMesher> mMesher;
	ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
    const Material* mStandardMaterial = nullptr;
    const Material* mShadowMaterial = nullptr;
    const Material* mLODMaterial = nullptr;
};

