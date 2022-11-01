#pragma once


DECL_VG(class SpriteBatch);

class CliWorldInterface;
class ResourceManager;
class Camera3D;
class Material;
class TileContainer;
class Mesh;
class ChunkGrassQuadtree;

// TODO: IRendererBase?
// TODO: DELETE ME
class TileContainerRenderer {
public:
	TileContainerRenderer();
	~TileContainerRenderer();

    void renderStaticMeshes(const std::set<const Mesh*>& meshes, const Camera3D& camera);
    void renderBillboards(const std::set<const Mesh*>& meshes, const Camera3D& camera);
    void renderWorldShadows(const std::set<const Mesh*>& meshes, const Camera3D& camera, f32 maxDistance);

private:

    const Material* mShadowMapperMaterial = nullptr;
    const Material* mShadowMapperMaterialBillboard = nullptr;
    const Material* mStandardMaterial = nullptr;
    const Material* mBillboardMaterial = nullptr;

    // TODO: Unused?
    CliWorldInterface* mCliWorld = nullptr;
};

