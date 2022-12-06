#pragma once

class MaterialRenderer;
class MaterialShader;
class HeightmapTerrainQuadtree;
class Camera3D;
class TerrainMesh;


class TerrainRenderer
{
public:
    TerrainRenderer();

    void renderTerrain(const Camera3D& camera, const std::set<const TerrainMesh*>& terrainMeshes);
    void renderWater(const Camera3D& camera, const std::set<const TerrainMesh*>& waterMeshes);

private:
    const MaterialShader* mWaterMaterial = nullptr;
    const MaterialShader* mTerrainMaterial = nullptr;
};

