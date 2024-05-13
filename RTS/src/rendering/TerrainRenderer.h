#pragma once

#include <boost/container/flat_set.hpp>

#include "resources/asset/AssetHandleBundle.h"
#include "world/road/TerrainTextureType.h"

class MaterialShaderDef;
class Camera3D;
class TerrainMesh;
class CubemapDef;
class World;
class WeatherManager;


class TerrainRenderer
{
public:
    TerrainRenderer();

    void setActiveWorld(World& world);

    void renderTerrain(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& terrainMeshes);
    void renderWater(const Camera3D& camera, const boost::container::flat_set<const TerrainMesh*>& waterMeshes, const CubemapDef& skyCubeMap);

private:
    void buildSurfaceDensityGradientMaps();

    const MaterialShaderDef* mWaterMaterial = nullptr;
    const MaterialShaderDef* mWaterPbrMaterial = nullptr;
    const MaterialShaderDef* mTerrainMaterial = nullptr;
    AssetHandleBundle mShaderAssets;
    // Owned by the world
    VGTexture mBiomeTexture = 0;
    f32 mInverseWorldWidth = 0.0f;
    WeatherManager* mWeatherManager = nullptr;

    // TODO: SSBO instead
    ui32 mMaterialsLookup[e_count(TerrainTextureType)] = {};
    VGTexture mSurfaceDensityGradientMapsArray = 0;
};

