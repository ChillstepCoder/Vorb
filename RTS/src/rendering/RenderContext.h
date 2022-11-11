#pragma once

class AmbientOcclusionPostProcess;
class BuildingRenderer;
class Camera3D;
class CliWorldInterface;
class CharacterRenderer;
class TileContainerRenderer;
class ChunkGrassQuadtree;
class CityDebugRenderer;
class CloudRenderer;
class GrassRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class CameraController;
class ICamera;
class ItemRenderer;
class LightRenderer;
class Material;
class MaterialRenderer;
class ParticleSystemRenderer;
class ResourceManager;
class ShadowRenderer;
class Skybox;
class TileContainer;
class TerrainRenderer;
class TerrainMeshManager;
class TextRenderer;
class UIContext;
class Mesh;
class QuadMesh;
class TerrainMesh;
class GrassMesh;
class CloudManager;
class RenderState;
class InstancedStaticModelRenderer;

struct SDL_Window;

#include <Vorb/graphics/GBuffer.h>
#include "rendering/GlobalUboData.h"

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);

struct GlobalRenderData {
    GlobalUboData globalUboData;
    f32 cameraZAngle;
    f32m4 skyRotMatrix;
    const f32m4* shadowFrustumMatrices;
    const f32* shadowCascadePlaneDistances;
    VGTexture shadowMap;
    ui32 shadowFrustumMatricesCount;
    const ICamera* mainCamera = nullptr;
};

constexpr ui32 INVALID_MESH_INDEX = UINT32_MAX;

// TODO: This is not cache freindly, ideally we just have a massive array of VAOs and loop through them
struct TileContainerMeshData {
    TileContainerMeshData() = default;
    ~TileContainerMeshData();

    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainerMeshData);

    std::unique_ptr<Mesh> mStaticMesh;
    std::unique_ptr<Mesh> mDynamicMesh;
    std::unique_ptr<Mesh> mBillboardMesh;
};

// Singleton
class RenderContext {
    friend class RenderThreadTasks;
protected:
    RenderContext(const f32v2& screenResolution, SDL_Window* window);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(const f32v2& screenResolution, SDL_Window* window);
    static RenderContext& getInstance();
    static RenderContext* tryGetInstance() { return sInstance; }
    static bool exists() { return sInstance != nullptr; }

    void onWorldBegin(const f32v2& worldCenter);

    void initPostLoad();

    void beginFrame(const Camera3D* camera, f32v3 playerPos); // Called automatically by beginFrame
    void renderFrame(CameraController& camera, f32 frameAlpha, f32 elapsedSec);
    void endFrame();

    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    TileContainerRenderer& getChunkRenderer() const { return *mTileContainerRenderer; }
    const vg::GBuffer& getActiveGBuffer() const { return *mActiveGBuffer; }
    const vg::GBuffer& getPrevFinalGBuffer() const { return mGBuffers[mPrevGBufferIndex]; }
    const f32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }
    VGTexture getShadowTexture() const;
    VGTexture getSSAOTexture() const;
    vg::SpriteFont& getSpriteFont() const { return *mSpriteFont; }
    vg::SpriteBatch& getSpriteBatch() const { return *mSb; }
    const f32v2& getScreenResolution() const { return mScreenResolution;}
    const Camera3D* getCamera() const { return mCamera; }

    // Meshing
    void addTerrainMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainMeshes.insert(mesh); }
    void removeTerrainMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainMeshes.erase(mesh); }
    void addTerrainWaterMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.insert(mesh); }
    void removeTerrainWaterMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.erase(mesh); }
    void addGrassMesh(const GrassMesh* mesh) { assert(IS_RENDER_THREAD()); mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { assert(IS_RENDER_THREAD()); mGrassMeshes.erase(mesh); }
    
    // Static models
    void addStaticModelInstance(ModelID id, const f32v3& pos, f32 rotation);

    // Character models
    CharacterRenderer& getCharacterRenderer() { return *mCharacterRenderer; }
private:
    void updateRenderThreadProcs();
    void renderDebug(const Camera3D& camera, const RenderState& renderState);
    void renderUI(const Camera3D& camera, const RenderState& renderState);
    void buildHorizonMesh();

    void addStaticMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mStaticMeshes.insert(mesh); }
    void removeStaticMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mStaticMeshes.erase(mesh); }
    void addDynamicMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mDynamicMeshes.insert(mesh); }
    void removeDynamicMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mDynamicMeshes.erase(mesh); }
    void addBillboardMesh(const Mesh* mesh);
    void removeBillboardMesh(const Mesh* mesh);

    static RenderContext* sInstance;
    
    // Client world
    CliWorldInterface* mCliWorld = nullptr;

    // Data
    GlobalRenderData mRenderData;
    f32v2 mScreenResolution;
    f32v2 mCurrentFramebufferDims;
    const Camera3D* mCamera = nullptr;

    // Renderers
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

    // Clouds
    std::unique_ptr<CloudManager> mCloudManager;

    // Mesh management
    std::map<TileContainer*, TileContainerMeshData> mTileContainerMeshData;
    // TODO: Profile vector instead (linear removal vs logn but better iteration performance)
    std::set<const Mesh*> mStaticMeshes;
    std::set<const Mesh*> mDynamicMeshes;
    std::set<const Mesh*> mBillboardMeshes;
    std::set<const GrassMesh*> mGrassMeshes;
    std::set<const TerrainMesh*> mTerrainMeshes;
    std::set<const TerrainMesh*> mTerrainWaterMeshes;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;
    SDL_Window* mWindow;

    int mPrevGBufferIndex = 1;
    int mActiveGBufferIndex = 0;
    vg::GBuffer* mActiveGBuffer = nullptr;
    vg::GBuffer mGBuffers[2];
    vg::GBuffer mTransparencyGBuffer;
    std::unique_ptr<Mesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;
    VGBuffer mGlobalUbo = 0;

    int mPassthroughRenderMode = 0;
    std::vector<const Material*> mPassthroughMaterials;
    const Material* mPassthroughMaterial = nullptr;
    const Material* mSceneLightingMaterial = nullptr;
    const Material* mCopyDepthMaterial = nullptr;
};

