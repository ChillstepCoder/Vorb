#pragma once

#include "world/WorldType.h"

class AmbientOcclusionPostProcess;
class Camera3D;
class CharacterRenderer;
class CityDebugRenderer;
class CloudManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GrassRenderer;
class InstancedStaticModelRenderer;
class ItemRenderer;
class InstancedStaticModelGatherer;
class IWorld;
class Skybox;
class LightRenderer;
class ParticleSystemRenderer;
class ShadowRenderer;
class SmudgeRenderer;
class TerrainRenderer;
class TileContainerRenderer;
class TonemapRenderer;
class RenderState;
class MaterialShader;
class Mesh;
class TerrainMesh;
class GrassMesh;

struct GlobalRenderData;

DECL_VG(class GBuffer);

class WorldRenderer
{
public:
    WorldRenderer(const f32v2& screenResolution);
    ~WorldRenderer();

    void initPostLoad();
    void onBeginFrame(const RenderState* renderState, const Camera3D* camera, f32v3 playerPos);
    void renderWorld(const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer, f32 frameAlpha, f32 elapsedSec);
    void renderDebug();

    // Renderer accessors
    TileContainerRenderer& getTileContainerRenderer() { return *mTileContainerRenderer; }
    CharacterRenderer& getCharacterRenderer() { return *mCharacterRenderer; }
    ShadowRenderer& getShadowRenderer() { return *mShadowRenderer; }

    // Assets
    void addStaticModelInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void addTerrainMesh(const TerrainMesh* mesh) {
        assert(IS_RENDER_THREAD()); mTerrainMeshes.insert(mesh);
    }
    void removeTerrainMesh(const TerrainMesh* mesh) {
        assert(IS_RENDER_THREAD()); mTerrainMeshes.erase(mesh);
    }
    void addTerrainWaterMesh(const TerrainMesh* mesh) {
        assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.insert(mesh);
    }
    void removeTerrainWaterMesh(const TerrainMesh* mesh) {
        assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.erase(mesh);
    }
    void addGrassMesh(const GrassMesh* mesh) {
        assert(IS_RENDER_THREAD()); mGrassMeshes.insert(mesh);
    }
    void removeGrassMesh(const GrassMesh* mesh) {
        assert(IS_RENDER_THREAD()); mGrassMeshes.erase(mesh);
    }

    void selectNextDebugShader();
    const std::string& getCurrentPassthroughRenderStageName() const;

    // Queries
    ui32 getNumStaticModels() const;

private:

    // Render passes
    void renderPassSky();
    void renderPassShadows(const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer);
    void renderPassTransparent();

    void buildHorizonMesh();

    mutable std::unique_ptr<TileContainerRenderer> mTileContainerRenderer;
    mutable std::unique_ptr<LightRenderer> mLightRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;
    mutable std::unique_ptr<ParticleSystemRenderer> mParticleSystemRenderer;
    mutable std::unique_ptr<CityDebugRenderer> mCityDebugRenderer;
    mutable std::unique_ptr<ItemRenderer> mItemRenderer;
    mutable std::unique_ptr<CharacterRenderer> mCharacterRenderer;
    mutable std::unique_ptr<CloudRenderer> mCloudRenderer;
    mutable std::unique_ptr<DepthOfFieldPostProcess> mDepthOfField;
    mutable std::unique_ptr<AmbientOcclusionPostProcess> mAmbientOcclusion;
    mutable std::unique_ptr<ShadowRenderer> mShadowRenderer;
    mutable std::unique_ptr<TerrainRenderer> mTerrainRenderer;
    mutable std::unique_ptr<GrassRenderer> mGrassRenderer;
    mutable std::unique_ptr<InstancedStaticModelRenderer> mStaticModelRenderer;
    mutable std::unique_ptr<SmudgeRenderer> mSmudgeRenderer;
    mutable std::unique_ptr<TonemapRenderer> mTonemapRenderer;
    std::unique_ptr<CloudManager> mCloudManager;

    // TODO: Profile vector instead (linear removal vs logn but better iteration performance)
    // TODO: WorldRenderData
    std::set<const GrassMesh*> mGrassMeshes;
    std::set<const TerrainMesh*> mTerrainMeshes;
    std::set<const TerrainMesh*> mTerrainWaterMeshes;
    std::unique_ptr<Mesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;

    std::unique_ptr<vg::GBuffer> mHDRLightGBuffer;

    f32v2 mScreenResolution;
    f32v3 mPlayerPos = f32v3(0.0f);
    const Camera3D* mCamera = nullptr;
    const RenderState* mRenderState = nullptr;
    IWorld* mActiveWorld = nullptr;
    std::set<IWorld*> mWorlds; // TODO: Per world data

    int mPassthroughRenderMode = 0;
    std::vector<const MaterialShader*> mPassthroughMaterials;
    const MaterialShader* mPassthroughMaterial = nullptr;
    const MaterialShader* mSceneLightingMaterial = nullptr;
    const MaterialShader* mCopyDepthMaterial = nullptr;
};

