#pragma once

#include "world/WorldType.h"

class AmbientOcclusionPostProcess;
class Camera3D;
class CharacterRenderer;
class CityDebugRenderer;
class CloudMeshManager;
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
class WorldRenderDataManager;

struct WorldRenderData;
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
    WorldRenderDataManager* tryGetRenderDataManagerForWorld(const IWorld& world) const;
    WorldRenderDataManager& getRenderDataManagerForWorld(const IWorld& world);

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

    // Renderers
    // TODO: Remove mutable?
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

    // World Data
    WorldRenderDataManager* mCurrentWorldRenderDataManager = nullptr;
    std::unordered_map<const IWorld*, std::unique_ptr<WorldRenderDataManager>> mRenderDataManagers;
    std::unique_ptr<Mesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;

    std::unique_ptr<vg::GBuffer> mHDRLightGBuffer;

    f32v2 mScreenResolution;
    f32v3 mPlayerPos = f32v3(0.0f);
    const Camera3D* mCamera = nullptr;
    const RenderState* mRenderState = nullptr;
    IWorld* mActiveWorld = nullptr;

    int mPassthroughRenderMode = 0;
    std::vector<const MaterialShader*> mPassthroughMaterials;
    const MaterialShader* mPassthroughMaterial = nullptr;
    const MaterialShader* mSceneLightingMaterial = nullptr;
    const MaterialShader* mCopyDepthMaterial = nullptr;
};

