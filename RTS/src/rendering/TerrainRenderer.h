#pragma once

class MaterialRenderer;
class Material;
class ResourceManager;
class HeightmapTerrainQuadtree;
class Camera3D;

class TerrainRenderer
{
public:
    TerrainRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2& gbufferDims);

    void renderTerrain(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees);
    void renderWater(const Camera3D& camera, const std::vector<HeightmapTerrainQuadtree>& terrainQuadtrees);

private:
    const MaterialRenderer& mMaterialRenderer;
    const Material* mWaterMaterial = nullptr;
    const Material* mTerrainMaterial = nullptr;
};

