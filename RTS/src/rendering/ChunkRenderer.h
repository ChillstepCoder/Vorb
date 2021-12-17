#pragma once

#include "world/Chunk.h"

DECL_VG(class SpriteBatch);

class ResourceManager;
class Camera3D;
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

    void renderChunksZCutout(const World& world, const Camera3D& camera);
    void renderWorld(const World& world, const Camera3D& camera, ChunkRenderLOD lod);
    void renderWorldShadows(const World& world, const Camera3D& camera, ChunkRenderLOD lod, f32 maxDistance);
    //void renderWorldShadows(const World& world, const Camera2D& camera);

    void InitPostLoad();

    ChunkMesher& getMesher() { return *mMesher; }
private:
    // Different rendering methods
    void TryRenderBaseMesh(const Chunk& chunk, const Material* material);
    void TryRenderGrassMeshes(const Chunk& chunk, const Material* material, const Camera3D& camera);
    void TryRenderBillboardMesh(const Chunk& chunk, const Material* material);
    //void RenderMeshOrLODTexture(const Chunk& chunk, const Camera3D& camera);
    void RenderLODTexture(const f32v2& worldPos, VGTexture texture, f32 width, const Camera3D& camera);
    void RenderLODTextureBindless(const f32v2& worldPos, VGTexture texture, f32 width, const Camera3D& camera, ui32 textureIndex);
    //void RenderShadows(const Chunk& chunk, const Camera2D& camera);

	ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
    const Material* mShadowMapperMaterial = nullptr;
    const Material* mShadowMapperMaterialBillboard = nullptr;
    const Material* mStandardMaterial = nullptr;
    const Material* mBillboardMaterial = nullptr;
    const Material* mLODMaterial = nullptr;
    const Material* mZCutoutMaterial = nullptr;
    const Material* mGrassMaterial = nullptr;

    std::vector<const Chunk*> mLODedChunksToRender;

    std::unique_ptr<ChunkMesher> mMesher;
};

