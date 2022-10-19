#pragma once

class MaterialRenderer;
class Material;
class HeightmapTerrainQuadtree;
class Camera3D;
class TerrainMesh;


class TerrainRenderer
{
public:
    TerrainRenderer(const MaterialRenderer& materialRenderer);

    void renderTerrain(const Camera3D& camera, const std::set<const TerrainMesh*>& terrainMeshes);
    void renderWater(const Camera3D& camera, const std::set<const TerrainMesh*>& waterMeshes);

private:
    const MaterialRenderer& mMaterialRenderer;
    const Material* mWaterMaterial = nullptr;
    const Material* mTerrainMaterial = nullptr;
};

