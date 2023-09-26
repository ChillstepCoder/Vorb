#pragma once

#include <boost/container/flat_set.hpp>

#include "resources/asset/AssetHandleBundle.h"

class MaterialShaderDef;
class Camera3D;
class TerrainMesh;
class CubemapDef;


class TerrainRenderer
{
public:
    TerrainRenderer();

    void renderTerrain(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& terrainMeshes);
    void renderWater(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& waterMeshes, const CubemapDef& skyCubeMap);

private:
    const MaterialShaderDef* mWaterMaterial = nullptr;
    const MaterialShaderDef* mWaterPbrMaterial = nullptr;
    const MaterialShaderDef* mTerrainMaterial = nullptr;
    AssetHandleBundle mShaderAssets;
};

