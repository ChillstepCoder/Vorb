#pragma once

class MaterialRenderer;
class Material;
class HeightmapTerrainQuadtree;
class Camera3D;

class TerrainRenderer
{
public:
    TerrainRenderer(const MaterialRenderer& materialRenderer);

    void renderTerrain(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees);
    void renderWater(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees);

private:
    const MaterialRenderer& mMaterialRenderer;
    const Material* mWaterMaterial = nullptr;
    const Material* mTerrainMaterial = nullptr;
};

