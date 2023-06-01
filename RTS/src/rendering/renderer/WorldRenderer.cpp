#include "stdafx.h"
#include "WorldRenderer.h"

#include <Vorb/graphics/DepthState.h>

#include "rendering/MaterialRenderer.h"
#include "rendering/StencilBufferIDs.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/CityDebugRenderer.h"
#include "rendering/CloudRenderer.h"
#include "rendering/EntityComponentSystemRenderer.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/model/InstancedStaticModelRenderer.h"
#include "rendering/ParticleSystemRenderer.h"
#include "rendering/post_process/AmbientOcclusionPostProcess.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/post_process/SmudgeRenderer.h"
#include "rendering/post_process/TonemapRenderer.h"
#include "rendering/renderer/GrassRenderer.h"
#include "rendering/renderer/OverlayRenderer.h"
#include "rendering/TerrainRenderer.h"
#include "rendering/TileContainerRenderer.h"
#include "rendering/Skybox.h"
#include "rendering/renderstate/RenderState.h"
#include "rendering/GlobalRenderData.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "rendering/mesh/TileContainerMeshManager.h"
#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "rendering/fish/FishRenderer.h"
#include "debugging/DebugRenderer.h"

#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"

#include "ui/UIContext.h"

#include "camera/Camera3D.h"
#include "physics/PhysicsWorld.h"

#include "tile/TileContainerRepository.h"

#include "structure/StructureManager.h"
#include "city/City.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderManager.h"

#include "weather/CloudMeshManager.h"

#include "time/TimeOfDayManager.h"

#include "options/DebugOptions.h"

#include "world/IWorld.h"

// TODO: Instead of single shader these should be able to be shader chains.
const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    "depth_debug",
    //"motion_blur",
    "normals",
    "shadow_depth_debug",
    "roughness_debug",
};

WorldRenderer::WorldRenderer(const f32v2& screenResolution) : mScreenResolution(screenResolution) {

    mCharacterRenderer = std::make_unique<CharacterRenderer>();
    mStaticModelRenderer = std::make_unique<InstancedStaticModelRenderer>();
    mTileContainerRenderer = std::make_unique<TileContainerRenderer>();
    mLightRenderer = std::make_unique<LightRenderer>();
    mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>();
    mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(screenResolution);
    mCityDebugRenderer = std::make_unique<CityDebugRenderer>();
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

    //mCloudManager->init(world.getLoadCenter());
}

WorldRenderer::~WorldRenderer()
{
}

void WorldRenderer::initPostLoad() {

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    MaterialShaderManager& materialManager = resourceManager.getMaterialShaderManager();
    {
        ScopedTimer timer("Skybox init", 2);
        buildHorizonMesh();
        mSkyBox = std::make_unique<Skybox>();
        mSkyBox->init(materialManager.getMaterialShader("sky"), &resourceManager.getTextureRepository().getCubemap("graycloud"));
    }

    // Init all passthrough materials
    {
        ScopedTimer timer("Passthrough init", 2);
        for (int i = 0; i < std::size(sPassthroughMaterialNames); ++i) {
            const MaterialShader* material = materialManager.getMaterialShader(sPassthroughMaterialNames[i]);
            if (material) {
                mPassthroughMaterials.emplace_back(material);
            }
            else {
                pError("Missing material for pass through: " + std::string(sPassthroughMaterialNames[i]));
            }
        }
    }

    mSceneLightingMaterial = materialManager.getMaterialShader("scene_lighting");
    mCopyDepthMaterial = materialManager.getMaterialShader("copy_depth");
    mPassthroughMaterial = materialManager.getMaterialShader("pass_through");
}

void WorldRenderer::onBeginFrame(const RenderState* renderState, f32v3 playerPos) {
    if (!renderState->getWorld()) {
        return;
    }
    if (mActiveWorld != renderState->getWorld()) {
        mActiveWorld = renderState->getWorld();
    }
    // Allocate render data if needed
    {
        auto&& it = mRenderDataManagers.find(mActiveWorld);
        if (it == mRenderDataManagers.end()) {
            mCurrentWorldRenderDataManager = mRenderDataManagers.insert(
                std::make_pair(mActiveWorld, std::make_unique<WorldRenderDataManager>(*mActiveWorld))
            ).first->second.get();
        }
        else {
            mCurrentWorldRenderDataManager = it->second.get();
        }
    }

    mRenderState = renderState;
    mPlayerPos = playerPos;

}

void WorldRenderer::renderWorld(const Camera3D* camera, const GlobalRenderData& renderData, vg::GBuffer* activeGBuffer, f32 frameAlpha, f32 elapsedSec, vg::GBuffer* targetGBuffer) {
    if (!mActiveWorld) {
        return;
    }
    mCamera = camera;

    // Any per frame world render data
    mCurrentWorldRenderDataManager->frameUpdate(*camera);

    // Sun
    const f32v3& sun = mActiveWorld->getTimeOfDayManager().getSunPosition();
    mShadowRenderer->beginFrame(*camera, sun);

    const TileContainerMeshManager& tileContainerMeshManager = mCurrentWorldRenderDataManager->getTileContainerMeshManager();

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
    Services::ResourceManager::ref().getMaterialRepository().bindMaterialBuffer();
    mStaticModelRenderer->renderModelPass(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMapForRenderPass(MaterialRenderPassType::Default), *mCamera);

    // Fish
    if (sDebugOptions.mShowFish) {
        mFishRenderer->renderFishEcosystem(*mCamera, *mActiveWorld);
    }

    // Smudge
    {
        mSmudgeRenderer->beginSmudgePass(activeGBuffer);
        mStaticModelRenderer->renderModelPass(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getModelInstanceMapForRenderPass(MaterialRenderPassType::Smudge), *mCamera);
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

    // TODO: Particles

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
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const MaterialShader* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        MaterialRenderer::renderFullScreenQuad(*postMat);
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
    mLightRenderer->renderSunlight(*activeGBuffer, mShadowRenderer->getShadowTexture(), *mSkyBox->getCubemap());

    // Sky (PBR version)
    if (sDebugOptions.mUsingPBR) {
        renderPassSky();
    }

    renderPassTransparent();

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
        const MaterialShader* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        MaterialRenderer::renderFullScreenQuad(*postMat);
    }

}

void WorldRenderer::renderDebug()
{
    assert(mActiveWorld);
    // City Debug
    if (sDebugOptions.mCities) {
        const CityGraph& cities = mActiveWorld->getCityGraph();
        for (auto&& city : cities.mNodes) {
            mCityDebugRenderer->renderCityPlannerDebug(city->getCityPlanner());
            mCityDebugRenderer->renderCityBuilderDebug(city->getCityBuilder());
            mCityDebugRenderer->renderCityPlotterDebug(city->getCityPlotter());
            mCityDebugRenderer->renderCityQuartermasterDebug(city->getCityQuartermaster());
        }
        mCityDebugRenderer->finishRenderFrame();
    }
    else {
        mCityDebugRenderer->clearMeshes();
    }

    // Structure debug
    if (sDebugOptions.mStructureDebug) {
        mActiveWorld->getStructureManager().debugRender();
    }

    mEcsRenderer->renderBusinessDebug(*mActiveWorld, *mCamera);


    if (sDebugOptions.mChunkBoundaries) {
        for (const auto& chunkDebugState : mRenderState->getDebugChunks()) {
            color4 color = COLOR_WHITE;
            if (chunkDebugState.mList == DebugChunkListIndex::DESTROYING) {
                color = color4(1.0f, 0.0f, 0.0f);
            }
            else if (chunkDebugState.mFlags.isBitSet(DebugChunkFlags::IS_NAVMESHING)) {
                color = color4(1.0f, 0.0f, 1.0f);
            }
            else {
                switch (chunkDebugState.mState) {
                    case ChunkState::INVALID:
                        color = color4(0.5f, 0.5f, 0.5f);
                        break;
                    case ChunkState::WAITING_HEIGHT:
                        color = color4(1.0f, 1.0f, 0.0f);
                        break;
                    case ChunkState::LOADING_TILES:
                        color = color4(0.0f, 1.0f, 1.0f);
                        break;
                    case ChunkState::TILE_LOAD_FINISHED:
                        color = color4(0.0f, 0.0f, 1.0f);
                        break;
                    case ChunkState::WAITING_MESH_PHYSICS_NAV:
                        color = color4(0.0f, 0.5f, 1.0f);
                        break;
                    case ChunkState::READY:
                        color = color4(0.0f, 1.0f, 0.0f);
                        break;
                    default:
                        break;
                }
            }

            const f32v2 worldPos = chunkDebugState.mWorldPos;
            DebugRenderer::drawWireQuad(worldPos, f32v2(CHUNK_WIDTH), color);

            // Count refs
            constexpr f32 REF_BOX_WIDTH = 1.0f;
            constexpr ui32 REF_ROW_WIDTH = (CHUNK_WIDTH - 1) / REF_BOX_WIDTH;
            for (int i = 0; i < chunkDebugState.mRefCount; ++i) {
                DebugRenderer::drawWireQuad(worldPos + f32v2(REF_BOX_WIDTH) + f32v2(i % REF_ROW_WIDTH, (i / REF_ROW_WIDTH) * 2) * REF_BOX_WIDTH, f32v2(REF_BOX_WIDTH), color4(1.0f, 0.0f, 1.0f));
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
                const f32v3 containerCenter = it.second->getTileSpatialGrid().getWorldPosCenter3D();
                const f32v3& cameraPos = mCamera->getPosition();
                if (glm::length2(mCamera->getPosition() - containerCenter) <= SQ(NAVGRAPH_RENDER_DISTANCE)) {
                    // TODO: make this only work on host world
                    assert(false);
                    //mWorld.getNavWorld().debugDrawCoarseNavGraphForContainer(*container, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
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

WorldRenderDataManager* WorldRenderer::tryGetRenderDataManagerForWorld(const IWorld& world) const {
    if (IS_RENDER_THREAD()) {
        auto&& it = mRenderDataManagers.find(&world);
        if (it == mRenderDataManagers.end()) {
            return nullptr;
        }
        return it->second.get();
    }
    else {
        std::lock_guard lock(mRenderDataManagersMutex);
        auto&& it = mRenderDataManagers.find(&world);
        if (it == mRenderDataManagers.end()) {
            return nullptr;
        }
        return it->second.get();
    }
}

WorldRenderDataManager& WorldRenderer::getRenderDataManagerForWorld(const IWorld& world) {
    if (IS_RENDER_THREAD()) {
        auto&& it = mRenderDataManagers.find(&world);
        if (it == mRenderDataManagers.end()) {
            std::lock_guard lock(mRenderDataManagersMutex);
            return *mRenderDataManagers.insert(
                std::make_pair(&world, std::make_unique<WorldRenderDataManager>(*mActiveWorld))
            ).first->second;
        }
        return *it->second;
    }
    else {
        std::lock_guard lock(mRenderDataManagersMutex);
        auto&& it = mRenderDataManagers.find(&world);
        if (it == mRenderDataManagers.end()) {
            assert(false);
            throw std::exception("Invalid world on game thread query");
        }
        return *it->second;
    }
}

void WorldRenderer::selectNextDebugShader() {
    ++mPassthroughRenderMode;
    if (mPassthroughRenderMode >= mPassthroughMaterials.size()) {
        mPassthroughRenderMode = 0;
    }
}

const std::string& WorldRenderer::getCurrentPassthroughRenderStageName() const
{
    if (mPassthroughRenderMode == 0) {
        return std::string();
    }
    return sPassthroughMaterialNames[mPassthroughRenderMode];
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
                mTileContainerRenderer->renderWorldShadows(mCurrentWorldRenderDataManager->getTileContainerMeshManager().getStaticMeshes(), mShadowRenderer->getShaderData(), *mCamera, mShadowRenderer->getMaxDistance(ShadowLodDetail::High));
            }

            // Instanced models
            Services::ResourceManager::ref().getMaterialRepository().bindMaterialBuffer();
            if (!sDebugOptions.mHideModels) {
                mStaticModelRenderer->renderModelShadows(mCurrentWorldRenderDataManager->getInstancedStaticModelManager().getAllModelInstanceMaps(), mShadowRenderer->getShaderData(), *mCamera);
            }

            // TODO: Frustum cull
            if (!sDebugOptions.mDisableClouds) {
                mCloudRenderer->renderCloudShadows(mShadowRenderer->getShaderData(), mCurrentWorldRenderDataManager->getCloudMeshManager(), *mCamera, mShadowRenderer->getMaxDistance(ShadowLodDetail::Highest));
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

void WorldRenderer::renderPassTransparent() {
    // Render clouds without shadows
    if (!mCurrentWorldRenderDataManager) {
        return;
    }

    if (!sDebugOptions.mDisableClouds && !sDebugOptions.mWireframe) {
        mCloudRenderer->renderClouds(mCurrentWorldRenderDataManager->getCloudMeshManager(), mHDRLightGBuffer->getDepthStencilTexture(), mHDRLightGBuffer.get(), *mCamera, *mSkyBox->getCubemap());
    }

    // Water (No depth write)
    if (!sDebugOptions.mDisableWater && !sDebugOptions.mWireframe) {
        glEnable(GL_DEPTH_CLAMP);
        mTerrainRenderer->renderWater(*mCamera, mCurrentWorldRenderDataManager->getTerrainMeshManager().getTerrainWaterMeshes(), *mSkyBox->getCubemap());
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
