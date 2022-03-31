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
class WorldGrid;


// TODO: IRendererBase?
class ChunkRenderer {
public:
	ChunkRenderer(const WorldGrid& worldGrid, ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
	~ChunkRenderer();

    void renderTiles(const World& world, const Camera3D& camera);
    void renderGrass(const World& world, const Camera3D& camera, const f32v3& playerPos);
    void renderBillboards(const World& world, const Camera3D& camera);
    void renderWorldShadows(const World& world, const Camera3D& camera, f32 maxDistance);

    void InitPostLoad();

    ChunkMesher& getMesher() { return *mMesher; }
private:
    // Different rendering methods
    void TryRenderBaseMesh(const Chunk& chunk, const Material* material);
    void TryRenderGrassMeshes(const Chunk& chunk, const Material* material, const Camera3D& camera);
    void TryRenderBillboardMesh(const Chunk& chunk, const Material* material);

	ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
    const Material* mShadowMapperMaterial = nullptr;
    const Material* mShadowMapperMaterialBillboard = nullptr;
    const Material* mStandardMaterial = nullptr;
    const Material* mBillboardMaterial = nullptr;
    const Material* mGrassMaterial = nullptr;

    std::unique_ptr<ChunkMesher> mMesher;
};

