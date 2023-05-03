#pragma once

#include <boost/container/flat_set.hpp>

class MaterialShader;
class Camera3D;
class TerrainMesh;
class Cubemap;


class TerrainRenderer
{
public:
    TerrainRenderer();

    void renderTerrain(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& terrainMeshes);
    void renderWater(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& waterMeshes, const Cubemap& skyCubeMap);

private:
    const MaterialShader* mWaterMaterial = nullptr;
    const MaterialShader* mWaterPbrMaterial = nullptr;
    const MaterialShader* mTerrainMaterial = nullptr;
};

