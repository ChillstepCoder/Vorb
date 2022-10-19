#pragma once


DECL_VG(class SpriteBatch);

class CliWorldInterface;
class ResourceManager;
class Camera3D;
class MaterialRenderer;
class Material;
class TileContainer;
class Mesh;
class ChunkGrassQuadtree;

// TODO: IRendererBase?
// TODO: DELETE ME
class TileContainerRenderer {
public:
	TileContainerRenderer(const MaterialRenderer& materialRenderer);
	~TileContainerRenderer();

    void renderTiles(const std::set<const Mesh*>& meshes, const Camera3D& camera);
    void renderGrass(const std::set<const ChunkGrassQuadtree*>& grassQuadtrees, const Camera3D& camera, const f32v3& playerPos);
    void renderBillboards(const std::set<const Mesh*>& meshes, const Camera3D& camera);
    void renderWorldShadows(const std::set<const Mesh*>& meshes, const Camera3D& camera, f32 maxDistance);

    void InitPostLoad();

private:

    const MaterialRenderer& mMaterialRenderer;
    const Material* mShadowMapperMaterial = nullptr;
    const Material* mShadowMapperMaterialBillboard = nullptr;
    const Material* mStandardMaterial = nullptr;
    const Material* mBillboardMaterial = nullptr;
    const Material* mGrassMaterial = nullptr;

    // TODO: Unused?
    CliWorldInterface* mCliWorld = nullptr;
};

