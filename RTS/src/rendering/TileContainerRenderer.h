#pragma once


DECL_VG(class SpriteBatch);

class CliWorldInterface;
class ResourceManager;
class Camera3D;
class MaterialShader;
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

    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterialBillboard = nullptr;
    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mBillboardMaterial = nullptr;

    // TODO: Unused?
    CliWorldInterface* mCliWorld = nullptr;
};

