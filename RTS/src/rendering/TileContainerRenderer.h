#pragma once

#include <boost/container/flat_set.hpp>
#include "resources/asset/AssetHandleBundle.h"

class Camera3D;
class MaterialShaderDef;
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

    const MaterialShaderDef* mShadowMapperMaterial = nullptr;
    const MaterialShaderDef* mShadowMapperMaterialBillboard = nullptr;
    const MaterialShaderDef* mStandardMaterial = nullptr;
    const MaterialShaderDef* mBillboardMaterial = nullptr;
    AssetHandleBundle mShaderAssets;

};

