#include "stdafx.h"
#include "WorldRenderer.h"

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/GBuffer.h>

#include "ecs/IFullECS.h"
#include "ecs/component/SkillsComponent.h"
#include "effect/IEffectContext.h"

// For debug rendering
#include "world/simulation/host/HostSimContext.h"

#include "debugging/DebugRenderer.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/CloudRenderer.h"
#include "rendering/ECSRenderer.h"
#include "rendering/fish/FishRenderer.h"
#include "rendering/GlobalRenderData.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/TileContainerMeshManager.h"
#include "rendering/model/InstancedDynamicModelRenderer.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "rendering/model/InstancedStaticModelRenderer.h"
#include "rendering/post_process/AmbientOcclusionPostProcess.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/post_process/SmudgeRenderer.h"
#include "rendering/post_process/TonemapRenderer.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/renderer/GrassRenderer.h"
#include "rendering/renderer/OverlayRenderer.h"
#include "rendering/renderstate/WorldRenderState.h"
#include "rendering/renderstate/GameRenderStateManager.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/Skybox.h"
#include "rendering/StencilBufferIDs.h"
#include "rendering/TerrainRenderer.h"
#include "rendering/TileContainerRenderer.h"

#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"

#include "ui/UIContext.h"

#include "camera/Camera3D.h"
#include "physics/PhysicsWorld.h"

#include "tile/TileContainerRepository.h"

#include "building/BuildingGrid.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/CubemapRepository.h"
#include "rendering/MaterialShaderRepository.h"

#include "weather/CloudMeshManager.h"

#include "time/TimeOfDayManager.h"

#include "options/DebugOptions.h"

#include "world/World.h"
#include "pathfinding/NavWorld.h"

// TODO: Instead of single shader these should be able to be shader chains.
constexpr StrToken sPassthroughMaterialNames[] = {
    CStrToken("pass_through"),
    CStrToken("depth_debug"),
    //"motion_blur",
    CStrToken("normals"),
    CStrToken("shadow_depth_debug"),
    CStrToken("roughness_debug"),
};

WorldRenderer::WorldRenderer(const f32v2& screenResolution) : mScreenResolution(screenResolution) {

    mCharacterRenderer = std::make_unique<CharacterRenderer>();
    mStaticModelRenderer = std::make_unique<InstancedStaticModelRenderer>();
    mDynamicModelRenderer = std::make_unique<InstancedDynamicModelRenderer>();
    mTileContainerRenderer = std::make_unique<TileContainerRenderer>();
    mLightRenderer = std::make_unique<LightRenderer>();
    mEcsRenderer = std::make_unique<ECSRenderer>();
    mItemRenderer = std::make_unique<ItemRenderer>();
    mCloudRenderer = std::make_unique<CloudRenderer>(screenResolution);
    mDepthOfField = std::make_unique<DepthOfFieldPostProcess>(screenResolution);
    mAmbientOcclusion = std::make_unique<AmbientOcclusionPostProcess>(screenResolution);
    mShadowRenderer = std::make_unique<ShadowRenderer>(screenResolution);
    mTerrainRenderer = std::make_unique<TerrainRenderer>();
    mGrassRenderer = std::make_unique<GrassRenderer>();
    mSmudgeRenderer = std::make_unique<SmudgeRenderer>(screenResolution);
    mTonemapRenderer = std::make_unique<TonemapRenderer>();
    mFishRenderer = std::make_unique<FishRenderer>();
    mOverlayRenderer = std::make_unique<OverlayRenderer>();
    checkGlError("WorldRenderer::WorldRenderer");

    mHDRLightGBuffer = std::make_unique<vg::GBuffer>(screenResolution);
    mHDRLightGBuffer->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB16F);



    initEventHandlers();
}

WorldRenderer::~WorldRenderer()
{
}

void WorldRenderer::initPostLoad() {

    MaterialShaderRepository& shaderRepo = MaterialShaderRepository::get();
    {
        ScopedTimer timer("Skybox init", 2);
        buildHorizonMesh();
        mSkyBox = std::make_unique<Skybox>();
        mSkyBox->init(CubemapRepository::get().getAssetHandle(CStrToken("graycloud")));
    }

    // Init all passthrough materials
    {
        ScopedTimer timer("Passthrough init", 2);
        for (int i = 0; i < std::size(sPassthroughMaterialNames); ++i) {
            mPassthroughMaterials.emplace_back(shaderRepo.getAssetHandle(sPassthroughMaterialNames[i]));
        }
    }

    mSceneLightingMaterial = shaderRepo.getAssetHandle(CStrToken("scene_lighting"));
    mCopyDepthMaterial = shaderRepo.getAssetHandle(CStrToken("copy_depth"));
    mPassthroughMaterial = shaderRepo.getAssetHandle(CStrToken("pass_through"));
}

void WorldRenderer::onBeginFrame(const WorldRenderState* renderState, f32v3 playerPos) {

    World* world = World::tryGetWorld(renderState->getWorldId());
    if (!world) {
        setActiveWorld(nullptr);
        return;
    }
    if (mActiveWorld != world) {
        if (GameRenderStateManager::getInstance().isActiveWorld(world)) {
            setActiveWorld(world);
        }
        else {
            setActiveWorld(nullptr);
            return;
        }
    }

    // This happens when a world is shutting down. The render state may have a world pointer
    // but it can be invalid due to being shut down
    {
        std::lock_guard lock(mRenderDataManagersMutex);
        auto&& it = mRenderDataManagers.find(mActiveWorld);
        if (it == mRenderDataManagers.end()) {
            if (mActiveWorld) {
                setActiveWorld(nullptr);
            }
            return;
        }
        else {
            mCurrentWorldRenderDataManager = it->second.get();
        }
    }


    mRenderState = renderState;
    mPlayerPos = playerPos;

    mCharacterRenderer->frameBegin();

}

void WorldRenderer::renderWorld(const Camera3D* camera, const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer, f32 frameAlpha, f32 elapsedSec, vg::GBuffer* targetGBuffer) {
    if (!mActiveWorld) {
        return;
    }
    mCamera = camera;

    // Any per frame world render data
    mCurrentWorldRenderDataManager->frameUpdate(*camera, elapsedSec);

    // Sun
    const f32v3& sun = mActiveWorld->getTimeOfDayManager().getSunPosition();
    mShadowRenderer->beginFrame(*camera, sun);

    const TileContainerMeshManager& tileContainerMeshManager = mCurrentWorldRenderDataManager->getTileContainerMeshManager();

    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::REPLACE);
    // Mark everything we draw as geometry
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::GEOMETRY), 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    // Static meshes
    mTileContainerRenderer->renderStaticMeshes(tileContainerMeshManager.getStaticMeshes(), *mCamera);

    if (!sDebugOptions.mHideCharacters) {
        mCharacterRenderer->renderCharacters(*mCamera, mRenderState->getCharacterRenderState(), elapsedSec, frameAlpha);
    }

    // Instanced models
    mDynamicModelRenderer->prepareFrame(mRenderState->getDynamicModels(), *camera);

    MaterialRepository::get().bindMaterialBuffer();
    mStaticModelRenderer->renderModelPass(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMap(), *mCamera, MaterialRenderPassType::Default, nullptr);
    mDynamicModelRenderer->renderModelPass(MaterialRenderPassType::Default);

    // Fish
    if (sDebugOptions.mShowFish) {
        mFishRenderer->renderFishEcosystem(*mCamera, *mActiveWorld);
    }

    // Smudge
    {
        mSmudgeRenderer->beginSmudgePass(activeGBuffer);
        mStaticModelRenderer->renderModelPass(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMap(), *mCamera, MaterialRenderPassType::Smudge, nullptr);
        mDynamicModelRenderer->renderModelPass(MaterialRenderPassType::Smudge);
        if (!sDebugOptions.mHideGrass && !sDebugOptions.mWireframe) {
            glDisable(GL_CULL_FACE);
            mGrassRenderer->renderGrass(*mCamera, mPlayerPos, mCurrentWorldRenderDataManager->getGrassMeshManager().getGrassMeshes());
            glEnable(GL_CULL_FACE);
        }
        mSmudgeRenderer->renderSmudge(activeGBuffer, *mCamera);
    }

    // Render stockpiles
    mItemRenderer->render(*mCamera);

    // TODO: Render loose items


    // Ambient occlusion
    mAmbientOcclusion->render(activeGBuffer);

    // === Post AO passes ===
    // Grass + billboards

    mTileContainerRenderer->renderBillboards(tileContainerMeshManager.getBillboardMeshes(), *mCamera);


    // PRE SMUDGE GRASS PASS
    /*if (!sDebugOptions.mHideGrass) {
        mGrassRenderer->renderGrass(*mCamera, playerPos, mGrassMeshes);
    }*/
    // TODO: Where is this getting unset?
    glEnable(GL_CULL_FACE);

    // TODO Try re-enable ambient occlusion for terrain in a smart way?
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::TERRAIN), 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // Terrain
        if (!sDebugOptions.mDisableTerrain) {
            mTerrainRenderer->renderTerrain(*mCamera, mCurrentWorldRenderDataManager->getTerrainMeshManager().getTerrainMeshes());
        }
        glDisable(GL_STENCIL_TEST);
    }

    // SHADOWED PARTICLES
    //mActiveWorld->getEffectContext().renderEffects(elapsedSec, *mCamera);

    // Paint smudges
    mSmudgeRenderer->renderPaintNoise(activeGBuffer, *mCamera);

    // Clouds
    /* if (!sDebugOptions.mDisableClouds) {
        mCloudRenderer->renderClouds(mWorld.getCloudManager(), activeGBuffer, camera);
    }*/

    // Editor brushes
    UIContext::getInstance().renderEditorBrushDecals(*mCamera);

    // Horizon
    //mMaterialRenderer->renderMesh(*mHorizonQuad, *mResourceManager.getMaterialManager().getMaterial("simple_color"));


    renderPassShadows(renderData, activeGBuffer);

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // *** Post processes ***
    // TODO: Bloom note (from acerola) https://www.youtube.com/watch?v=IMiiUEG-sLQ_
    // Contrast -> Brighness -> Saturation -> Gamma correction -> Bloom -> Bloom can be done via mipmapping (GPU DOWNSCALING then UPSCALING)

    vg::DepthState::NONE.set();

    // Render characters that are behind geometry with some transparency
    //mEcsRenderer->renderCharacterModels(*mCharacterRenderer, camera, 0.20f, frameAlpha);
        // Depth debug
    if (mPassthroughRenderMode == 1) {
        vg::sBlendStates.ALPHA.set();
        const MaterialShaderDef* postMat = mPassthroughMaterials[mPassthroughRenderMode]->tryGetLoadedAsset();
        if (postMat) {

            // TODO: Swap chain for this to work
            MaterialRenderer::renderFullScreenQuad(*postMat);
        }
    }

    // Sky (non PBR version)
    if (!sDebugOptions.mUsingPBR) {
        renderPassSky();
    }

    // Final render for pre-transparency
    mHDRLightGBuffer->use();
    // Share values
    mHDRLightGBuffer->setTertiaryTexture(activeGBuffer->getTertiaryTexture());
    mHDRLightGBuffer->setNormalTexture(activeGBuffer->getNormalTexture());
    mHDRLightGBuffer->setSharedDepthStencilTexture(activeGBuffer->getDepthStencilTexture());

    // Sunlight
    const CubemapDef* cubemap = mSkyBox->tryGetCubemap();
    if (cubemap) {
        mLightRenderer->renderSunlight(*activeGBuffer, mShadowRenderer->getShadowTexture(), *cubemap);
    }

    // Sky (PBR version)
    if (sDebugOptions.mUsingPBR) {
        renderPassSky();
    }

    renderPassTransparent(elapsedSec);

    // Update active
    activeGBuffer = mHDRLightGBuffer.get();

    // Depth of field
    vg::DepthState::NONE.set();
    activeGBuffer = mDepthOfField->render(activeGBuffer);

    // Final render to screen or target GBuffer, applying tonemap
    if (targetGBuffer) {
        targetGBuffer->use();
    }
    else {
        activeGBuffer->unuse();
        glViewport(0, 0, mScreenResolution.x, mScreenResolution.y);
    }
    mTonemapRenderer->render(activeGBuffer->getAlbedoTexture());
    //MaterialRenderer::renderFullScreenQuad(*mPassthroughMaterial);

    if (sDebugOptions.mIsCameraUnderwater) {
        mOverlayRenderer->renderUnderwaterOverlay();
    }

    // Final Pass through process
    // TODO: Make this work. When in debug, render tonemap to a new texture
    // FBODebugRenderer?
    if (mPassthroughRenderMode > 1) {
        const MaterialShaderDef* postMat = mPassthroughMaterials[mPassthroughRenderMode]->tryGetLoadedAsset();
        if (postMat) {

            // TODO: Swap chain for this to work
            MaterialRenderer::renderFullScreenQuad(*postMat);
        }
    }

}

f32v3 helperGetWorldPosWithHeight(f32v2 worldPos, World* activeWorld) {
    f32v3 rv;
    rv.x = worldPos.x;
    rv.y = worldPos.y;
    rv.z = activeWorld->getHeightmapGrid().computeHeightAtPoint<true>(worldPos);
    return rv;
}

void WorldRenderer::renderDebug() {
    if (!mActiveWorld) {
        return;
    }

    if (const HostSimContext* simContext = mActiveWorld->tryGetHostSimContext()) {
        simContext->debugRender(mCamera->getPosition());
    }

    // Structure debug
    if (sDebugOptions.mBuildingDebug) {
        mActiveWorld->getBuildingGrid().debugRender();
    }

    mEcsRenderer->renderBusinessDebug(*mActiveWorld, *mCamera);

    if (sDebugOptions.mChunkBoundaries) {
        DebugRenderer::reserveLines(mRenderState->getDebugChunks().size() * 16);
        for (const auto& chunkDebugState : mRenderState->getDebugChunks()) {
            color4 color = COLOR_WHITE;
            if (chunkDebugState.mList == DebugChunkListIndex::DESTROYING) {
                color = color4(1.0f, 0.0f, 0.0f);
            }
            else if (chunkDebugState.mFlags.isBitSet(DebugChunkFlags::IS_NAVMESHING)) {
                color = color4(1.0f, 1.0f, 0.0f);
            }
            else {
                switch (chunkDebugState.mState) {
                    case ChunkState::DEACTIVATED:
                        color = color4(0.5f, 0.5f, 0.5f);
                        break;
                    case ChunkState::WAITING_SIM_RELEASE:
                        color = color4(0.0f, 0.0f, 1.0f);
                        break;
                    case ChunkState::READY_TO_LOAD:
                        color = color4(1.0f, 1.0f, 1.0f);
                        break;
                    case ChunkState::LOADING_TILES:
                        color = color4(0.0f, 1.0f, 1.0f);
                        break;
                    case ChunkState::WAITING_BUILDINGS:
                        color = color4(0.35f, 0.7f, 1.0f);
                        break;
                    case ChunkState::LOADING_MESH_PHYSICS_NAV_VISIBILITY:
                        color = color4(0.0f, 0.5f, 1.0f);
                        break;
                    case ChunkState::ACTIVATED:
                        color = color4(0.0f, 1.0f, 0.0f);
                        break;
                    case ChunkState::DESTROYING_ON_SIM:
                        color = color4(1.0f, 0.0f, 1.0f);
                        break;
                    default:
                        break;
                }
                static_assert(e_count(ChunkState) == 8);
            }

            f32v3 worldPosA, worldPosB;
            worldPosA = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos, mActiveWorld);
            worldPosB = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(CHUNK_WIDTH, 0.0f), mActiveWorld);
            DebugRenderer::drawLineBetweenPoints(worldPosA, worldPosB, color);

            worldPosA = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(CHUNK_WIDTH, 0.0f), mActiveWorld);
            worldPosB = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(CHUNK_WIDTH, CHUNK_WIDTH), mActiveWorld);
            DebugRenderer::drawLineBetweenPoints(worldPosA, worldPosB, color);

            worldPosA = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(CHUNK_WIDTH, CHUNK_WIDTH), mActiveWorld);
            worldPosB = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(0.0f, CHUNK_WIDTH), mActiveWorld);
            DebugRenderer::drawLineBetweenPoints(worldPosA, worldPosB, color);

            worldPosA = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos + i32v2(0.0f, CHUNK_WIDTH), mActiveWorld);
            worldPosB = helperGetWorldPosWithHeight(chunkDebugState.mWorldPos, mActiveWorld);
            DebugRenderer::drawLineBetweenPoints(worldPosA, worldPosB, color);
          
            // Count refs
            constexpr f32 REF_BOX_WIDTH = 1.0f;
            constexpr ui32 REF_ROW_WIDTH = (CHUNK_WIDTH - 1) / (ui32)REF_BOX_WIDTH;
            for (int i = 0; i < chunkDebugState.mRefCount; ++i) {
                DebugRenderer::drawWireQuad(f32v2(chunkDebugState.mWorldPos) + f32v2(REF_BOX_WIDTH) + f32v2(i % REF_ROW_WIDTH, (i / REF_ROW_WIDTH) * 2) * REF_BOX_WIDTH, f32v2(REF_BOX_WIDTH), color4(1.0f, 0.0f, 1.0f));
            }
        }
    }

    // Fish
    if (sDebugOptions.mDebugFishEcosystem) {
        mFishRenderer->debugRenderFishEcosystem(*mActiveWorld);
    }

    // Nav graph (Render is slow so we only build the line meshes when toggle changes)
    constexpr int NAVGRAPH_ID = 44432;
    constexpr f32 NAVGRAPH_RENDER_DISTANCE = 100.0f; // TODO: Move to debugoptions
    static bool wasRenderingNavGraph = false;
    if (sDebugOptions.mShowNavGraph) {
        if (!wasRenderingNavGraph) {
            ScopedTimer timer("Debug Draw Navgraph");
            DebugRenderer::reserveLines(mActiveWorld->getChunkGrid().getNumActiveChunks() * 1024, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
            const auto& containers = mActiveWorld->getTileContainerRepository().getTileContainers();
            for (auto&& it : containers) {
                const f32v3 containerCenter = it.second->getTileSpatialGrid().getWorldPosCenter();
                const f32v3 cameraPos = mCamera->getPosition();
                if (glm::length2(mCamera->getPosition() - containerCenter) <= SQ(NAVGRAPH_RENDER_DISTANCE)) {
                    
                    if (NavWorld* navWorld = mActiveWorld->tryGetNavWorld()) {
                        navWorld->debugDrawCoarseNavGraphForContainer(*it.second, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
                    }
                }
            }
            wasRenderingNavGraph = true;
        }
    }
    else if (wasRenderingNavGraph) {
        wasRenderingNavGraph = 0;
        DebugRenderer::clearAllMeshesWithId(NAVGRAPH_ID);
    }

    // Debug Shapes
    const std::vector<DebugWireQuadState>& debugQuads = mRenderState->getDebugQuads();
    for (const DebugWireQuadState& quad : debugQuads) {
        DebugRenderer::drawWireQuad(quad.origin, quad.dims, quad.color);
    }

    // Grass
    if (sDebugOptions.mDebugGrassLod) {
        std::vector<DebugWireQuadState> newDebugQuads;
        newDebugQuads.reserve(128);
        for (auto&& trackedChunk : mCurrentWorldRenderDataManager->getGrassMeshManager().getTrackedChunks()) {
            if (trackedChunk.quadtree) {
                trackedChunk.quadtree->getDebugQuads(newDebugQuads);
            }
        }
        for (const DebugWireQuadState& quad : newDebugQuads) {
            DebugRenderer::drawWireQuad(quad.origin, quad.dims, quad.color);
        }
    }

    // Axis labels
    if (sDebugOptions.mShowDevHud) {
        const f32v3 axisOrigin = mCamera->getPosition() + mCamera->getFrontVector() * 5.0f + mCamera->getRightVector() * -5.0f + mCamera->getUpVector() * 2.5f;
        DebugRenderer::drawLine(axisOrigin, f32v3(1.0f, 0.0f, 0.0f), color4(1.0f, 0.0f, 0.0f)); //X
        DebugRenderer::drawLine(axisOrigin, f32v3(0.0f, 1.0f, 0.0f), color4(0.0f, 1.0f, 0.0f)); //Y
        DebugRenderer::drawLine(axisOrigin, f32v3(0.0f, 0.0f, 1.0f), color4(0.0f, 0.0f, 1.0f)); //Z
    }

    // Physics
    mActiveWorld->getPhysicsWorld().debugRender();
}

WorldRenderDataManager& WorldRenderer::getRenderDataManagerForWorld(const World& world) {
    std::lock_guard lock(mRenderDataManagersMutex);
    auto&& it = mRenderDataManagers.find(&world);
    if (it == mRenderDataManagers.end()) {
        panic("Missing world in WorldRenderer::getRenderDataManagerForWorld");
    }
    return *it->second;
}

WorldRenderDataManager* WorldRenderer::tryGetRenderDataManagerForWorld(const World& world)
{
    WorldID id = world.getId();
    std::lock_guard lock(mRenderDataManagersMutex);
    auto&& it = mRenderDataManagers.find(&world);
    if (it == mRenderDataManagers.end()) {
        return nullptr;
    }
    return it->second.get();
}

void WorldRenderer::removeRenderDataManagerForWorld(const World& world) {
    {
        std::lock_guard lock(mRenderDataManagersMutex);
        auto&& it = mRenderDataManagers.find(&world);
        if (it != mRenderDataManagers.end()) {
            mRenderDataManagers.erase(it);
        }
    }
}

void WorldRenderer::selectNextDebugShader() {
    ++mPassthroughRenderMode;
    if (mPassthroughRenderMode >= mPassthroughMaterials.size()) {
        mPassthroughRenderMode = 0;
    }
}

StrToken WorldRenderer::getCurrentPassthroughRenderStageName() const
{
    if (mPassthroughRenderMode == 0) {
        return StrToken();
    }
    return sPassthroughMaterialNames[mPassthroughRenderMode];
}

void WorldRenderer::initEventHandlers() {
    World::registerStaticWorldListeners(mEventHandles.worldEventListeners);
    World::addOnWorldBeginGameThreadListener(mEventHandles.worldEventListeners, [this](World& world) {
        std::lock_guard lock(mRenderDataManagersMutex);
        mRenderDataManagers.insert(
            std::make_pair(&world, std::make_unique<WorldRenderDataManager>(world))
        );
        mCharacterRenderer->onWorldBegin(world);

    });
    World::addOnWorldEndRenderThreadListener(mEventHandles.worldEventListeners, [this](World& world) {
        WorldRenderDataManager* mgr = nullptr;
        {
            std::lock_guard lock(mRenderDataManagersMutex);
            auto&& it = mRenderDataManagers.find(&world);
            if (it != mRenderDataManagers.end()) {
                mgr = it->second.get();
            }
        }
        // Have to pull out of critical section as nested calls might lock mutex
        if (mgr) {
            mgr->shutdown();
        }
        // TODO: What if we are still holding on to the handle?

        mEventHandles.skillsComponentListeners.reset();
    });
}

void WorldRenderer::renderPassSky() {
    // glEnable(GL_STENCIL_TEST);
    // glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::SKY), 0xFF);
     //glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (sDebugOptions.mUsingPBR) {
        mSkyBox->renderPbr(mCamera->getVPMatrix());
    }
    else {
        mSkyBox->render(mCamera->getVPMatrix());
    }
    // glDisable(GL_STENCIL_TEST);
}

void WorldRenderer::renderPassShadows(const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer) {
    PROFILE_FUNCTION();
    const TimeOfDayManager& timeOfDayManager = mActiveWorld->getTimeOfDayManager();
    // TODO: SunHeight race condition
    if (timeOfDayManager.getSunHeight() > 0.01f && !sDebugOptions.mDisableShadows) {
        if (mShadowRenderer->shouldUpdateShadowsThisFrame()) {
            mShadowRenderer->useShadowBuffer();
            glEnable(GL_DEPTH_CLAMP);

            vg::DepthState::FULL.set();
            // Render all shadow casters
            //glCullFace(GL_FRONT);
            // TODO: Why is this labeled as terrain?
            if (!sDebugOptions.mDisableTerrain) {
                mTileContainerRenderer->renderWorldShadows(mCurrentWorldRenderDataManager->getTileContainerMeshManager().getStaticMeshes(), mShadowRenderer->getShaderData(), *mCamera, mShadowRenderer->getMaxDistance(ShadowDetail::High));
            }

            // Instanced models
            MaterialRepository::get().bindMaterialBuffer();
            if (!sDebugOptions.mHideModels) {
                mStaticModelRenderer->renderModelShadows(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMap(), mShadowRenderer->getShaderData(), *mCamera);
            }

            // TODO: Frustum cull
            if (!sDebugOptions.mDisableClouds) {
                mCloudRenderer->renderCloudShadows(mShadowRenderer->getShaderData(), mCurrentWorldRenderDataManager->getCloudMeshManager(), *mCamera, mShadowRenderer->getMaxDistance(ShadowDetail::Highest));
            }

            //const CityGraph& cities = sWorld->getCityGraph();
            //for (auto&& city : cities.mNodes) {
            //    const std::vector<std::unique_ptr<Building>>& buildings = city->getBuildings();
            //    for (auto& building : buildings) {
            //        mBuildingRenderer->renderBuildingShadows(*building, camera);
            //    }
            //}
            //for (auto&& structure : structures) {
            //    // TODO: List of buildings instead?
            //    if (structure->getType() == StructureType::Building) {
            //        mBuildingRenderer->renderBuildingShadows((Building&)*structure, camera);
            //    }
            //}

            glDisable(GL_DEPTH_CLAMP);
        }

        vg::DepthState::NONE.set();
        mShadowRenderer->renderShadows(mCamera->getPosition());
        activeGBuffer->use();
    }
    else {
        // No shadow bleed from previous frames
        mShadowRenderer->clearShadowTexture();
    }
}

void WorldRenderer::renderPassTransparent(f32 elapsedSec) {
    // Render clouds without shadows
    if (!mCurrentWorldRenderDataManager) {
        return;
    }
    const CubemapDef* cubeMap = mSkyBox->tryGetCubemap();
    if (!cubeMap) {
        return;
    }

    if (!sDebugOptions.mDisableClouds && !sDebugOptions.mWireframe) {
        mCloudRenderer->renderClouds(mCurrentWorldRenderDataManager->getCloudMeshManager(), mHDRLightGBuffer->getDepthStencilTexture(), mHDRLightGBuffer.get(), *mCamera, *cubeMap);
    }

    // Emissive Particles
    mActiveWorld->getEffectContext().renderEffects(elapsedSec, *mCamera);

    // Water (No depth write)
    if (!sDebugOptions.mDisableWater && !sDebugOptions.mWireframe) {
        glEnable(GL_DEPTH_CLAMP);
        mTerrainRenderer->renderWater(*mCamera, mCurrentWorldRenderDataManager->getTerrainMeshManager().getTerrainWaterMeshes(), *cubeMap);

        // Model water
        vg::DepthState::READ.set();
        vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);
        mStaticModelRenderer->renderModelPass(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMap(), *mCamera, MaterialRenderPassType::Water, cubeMap);
        vg::BlendState::restorePrevious();
        glDisable(GL_DEPTH_CLAMP);
    }


    // Light transparent layer

}


void WorldRenderer::buildHorizonMesh() {
    mHorizonQuad = std::make_unique<Mesh>();
    ProceduralMeshBuilder meshBuilder(true);
    constexpr float QUAD_WIDTH = 140000.0f;
    meshBuilder.addAxisAlignedQuad(f32v3(-QUAD_WIDTH, -QUAD_WIDTH, 0.0f), f32v2(QUAD_WIDTH * 2.0f), CubeFacing::TOP, MaterialDesc(), f32v4(0.0f, 0.0f, 1.0f, 1.0f), COLOR_WHITE);
    meshBuilder.finishMesh(mHorizonQuad, f32v3(0.0f));
}

void WorldRenderer::setActiveWorld(World* world) {
    mActiveWorld = world;

    // Skill events
    // TODO: This maybe shouldn't be  here?
    if (mActiveWorld) {

        // TODO: Rename -> setActiveWorld
        mTerrainRenderer->setActiveWorld(*mActiveWorld);
        mGrassRenderer->setActiveWorld(*mActiveWorld);
        mStaticModelRenderer->setActiveWorld(*mActiveWorld);

        SkillsComponentSystem& skillsSystem = mActiveWorld->getECS().mSkillsSystem;
        skillsSystem.registerSkillsComponentSystemListeners(mEventHandles.skillsComponentListeners);
        skillsSystem.addActivateListener(mEventHandles.skillsComponentListeners, [world](SkillEvent skillEvent) {
            ASSERT_GAME_THREAD();
            SkillsComponent& skillsCmp = world->getECS().mRegistry.get<SkillsComponent>(skillEvent.mEntity);
            const SkillDef* skill = skillsCmp.mSkills[e_cast(skillEvent.mSkillSlot)]->tryGetLoadedAsset();
            if (skill) {
                RenderThreadTasks::getInstance().playOneShotAnimation(skillEvent.mEntity, skill->mAnimation.getAssetID());
            }
            else {
                LOG_WARN("Tried to play skill animation for skill slot {} but skill was not loaded", (int)skillEvent.mSkillSlot);
            }
        });
    }
    else {
        mEventHandles.skillsComponentListeners.reset();
    }
}
