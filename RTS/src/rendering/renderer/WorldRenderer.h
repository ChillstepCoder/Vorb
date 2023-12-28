#pragma once

#include "events/SkillEvent.h"
#include "world/WorldEvents.h"

class AmbientOcclusionPostProcess;
class Camera3D;
class CharacterRenderer;
class CityDebugRenderer;
class CloudMeshManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GrassRenderer;
class InstancedDynamicModelRenderer;
class InstancedStaticModelRenderer;
class ItemRenderer;
class InstancedStaticModelGatherer;
class World;
class Skybox;
class LightRenderer;
class ParticleSystemRenderer;
class ShadowRenderer;
class SmudgeRenderer;
class TerrainRenderer;
class TileContainerRenderer;
class TonemapRenderer;
class WorldRenderState;
class MaterialShaderDef;
class Mesh;
class TerrainMesh;
class GrassMesh;
class WorldRenderDataManager;
class FishRenderer;
class OverlayRenderer;

struct WorldRenderData;
struct GlobalRenderData;

DECL_VG(class GBuffer);

// Manages both rendering and data for one or more worlds
// TODO: Split into data + render?
class WorldRenderer
{
public:
    WorldRenderer(const f32v2& screenResolution);
    ~WorldRenderer();

    void initPostLoad();
    void onBeginFrame(const WorldRenderState* renderState, f32v3 playerPos);
    void renderWorld(const Camera3D* camera, const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer, f32 frameAlpha, f32 elapsedSec, vg::GBuffer* targetGBuffer);
    void renderDebug();

    // Renderer accessors
    TileContainerRenderer& getTileContainerRenderer() { return *mTileContainerRenderer; }
    CharacterRenderer& getCharacterRenderer() { return *mCharacterRenderer; }
    ShadowRenderer& getShadowRenderer() { return *mShadowRenderer; }

    // Assets
    WorldRenderDataManager& getRenderDataManagerForWorld(const World& world);
    void removeRenderDataManagerForWorld(const World& world);

    void selectNextDebugShader();
    StrToken getCurrentPassthroughRenderStageName() const;

private:
    void initEventHandlers();

    // Render passes
    void renderPassSky();
    void renderPassShadows(const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer);
    void renderPassTransparent(f32 elapsedSec);

    void buildHorizonMesh();

    void setActiveWorld(World* world);

    // Renderers
    // TODO: Remove mutable?
    mutable std::unique_ptr<TileContainerRenderer> mTileContainerRenderer;
    mutable std::unique_ptr<LightRenderer> mLightRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;
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
    mutable std::unique_ptr<InstancedDynamicModelRenderer> mDynamicModelRenderer;
    mutable std::unique_ptr<SmudgeRenderer> mSmudgeRenderer;
    mutable std::unique_ptr<TonemapRenderer> mTonemapRenderer;
    mutable std::unique_ptr<FishRenderer> mFishRenderer;
    mutable std::unique_ptr<OverlayRenderer> mOverlayRenderer;

    // World Data
    WorldRenderDataManager* mCurrentWorldRenderDataManager = nullptr;
    mutable std::mutex mRenderDataManagersMutex;
    std::unordered_map<const World*, std::unique_ptr<WorldRenderDataManager>> mRenderDataManagers;
    std::unique_ptr<Mesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;

    std::unique_ptr<vg::GBuffer> mHDRLightGBuffer;

    f32v2 mScreenResolution;
    f32v3 mPlayerPos = f32v3(0.0f);
    const Camera3D* mCamera = nullptr;
    const WorldRenderState* mRenderState = nullptr;
    World* mActiveWorld = nullptr;

    int mPassthroughRenderMode = 0;
    std::vector< AssetHandlePtr<MaterialShaderDef>> mPassthroughMaterials;
    AssetHandlePtr<MaterialShaderDef> mPassthroughMaterial;
    AssetHandlePtr<MaterialShaderDef> mSceneLightingMaterial;
    AssetHandlePtr<MaterialShaderDef> mCopyDepthMaterial;

    // Event handles
    struct WorldRendererEventHandles {
        SkillsComponentSystemListeners skillsComponentListeners;
        WorldListeners worldEventListeners;
    } mEventHandles;

};

