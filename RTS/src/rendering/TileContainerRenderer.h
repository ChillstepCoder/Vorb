#pragma once

#include <boost/container/flat_set.hpp>

class Camera3D;
class MaterialShader;
class TileContainer;
class Mesh;
class InstancedStaticModelRenderer;
struct ShadowPassShaderData;

class TileContainerRenderer {
public:
	TileContainerRenderer();
	~TileContainerRenderer();

    void renderStaticMeshes(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera);
    void renderBillboards(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera);
    void renderWorldShadows(const boost::container::flat_set<const Mesh*>& meshes, const ShadowPassShaderData& shaderData, const Camera3D& camera, f32 maxDistance);

private:

    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterialBillboard = nullptr;
    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mBillboardMaterial = nullptr;


};

