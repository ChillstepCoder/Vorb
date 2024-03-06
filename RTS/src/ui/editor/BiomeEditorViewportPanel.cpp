#include "stdafx.h"
#include "BiomeEditorViewportPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "world/controller/EditorWorldInterfaceController.h"

#include "resources/ResourceManager.h"
#include "resources/BiomeRepository.h"

#include "ui/imgui_controls/ObjectVector.h"
#include "ui/imgui_controls/EnumCombo.h"

#include "world/World.h"
#include "world/WorldDestroyer.h"
#include "world/Chunk.h"
#include "world/host/HostWorldData.h"
#include "rendering/renderer/WorldRenderer.h"
#include "rendering/renderstate/GameRenderStateManager.h"
#include "rendering/RenderThreadTasks.h"

#include "rendering/RenderContext.h"

#include "gamethread/GameThread.h"
#include "gamethread/GameThreadTasks.h"
#include "time/TimeOfDayManager.h"

#include "camera/Camera3D.h"
#include "camera/SimpleCamera.h"

#include "App.h"

#include "ui/UIContext.h"

#include "rendering/MaterialShaderDef.h"

constexpr ui32 EDITOR_CHUNK_GRID_WIDTH = 2; // nxn grid
constexpr ui32 NUM_EDITOR_CHUNKS = SQ(EDITOR_CHUNK_GRID_WIDTH);
constexpr int LOCAL_GROUP_SIZE = 16;

BiomeEditorViewportPanel::BiomeEditorViewportPanel() {

}

BiomeEditorViewportPanel::~BiomeEditorViewportPanel()
{
    if (mHeightSSBO) {
        glUnmapNamedBuffer(mHeightSSBO);
        glDeleteBuffers(1, &mHeightSSBO);
        mHeightSSBO = 0;
    }
}

void BiomeEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
   
    // Tell the world to follow our camera
    if (mEditorWorld) {
        const f32v2 cameraPos = mCamera->getPosition();
        GameThreadTasks::getInstance().addGenericTask([cameraPos, editorWorld = mEditorWorld.get()]() {
            editorWorld->setLoadCenter(cameraPos);
        });
    }
}

void BiomeEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Biome Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Biome Editor Controls");

    bool changed = false;
    if (mAssetData) {
        ImGui::Text("Biome: %s", mAssetData->displayName.c_str());
        updateAndRenderSaveButton();
        if (ImGui::Button("Regenerate World")) {
            initializeWorld();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Camera")) {
            resetCamera();
        }
        ImGui::SliderFloat("Height Offset", &mHeightOffset, -100.f, 100.f);
        if (ImGui::CollapsingHeader("Properties")) {
            changed |= updateAndRenderImguiControls(*mAssetData);
            // Tile gen categories
            changed |= ImguiUtil::ObjectVector<BiomeTileGenCategory>("Categories", mAssetData->tileGenCategories,
                [](BiomeTileGenCategory& o, ui32) {
                bool changed = false;
                changed |= updateAndRenderImguiControls(o);
                changed |= ImguiUtil::ObjectVector<BiomePossibleTile>("Tiles", o.tiles,
                    [](BiomePossibleTile& o, ui32) {
                    bool changed = false;
                    if (o.tile.name.isValid()) {
                        ImGui::Text(o.tile.name.toString().c_str());
                    }
                    else {
                        ImGui::Text("INVALID");
                    }
                    if (ImGui::TreeNode("Distribution")) {
                        changed |= updateAndRenderImguiControls(o);
                        ImGui::TreePop();
                    }
                    if (!o.tile.isValid()) {
                        return changed;
                    }
                    AssetHandlePtr<TileDef> tileDefHandle = static_unique_pointer_cast<AssetHandle<TileDef>>(o.tile.getAssetHandle());
                    if (!tileDefHandle) {
                        ImGui::Text("!!!INVALID REFERENCE!!!");
                        return changed;
                    }
                    else {
                        const TileDef& tileDef = tileDefHandle->getLoadedAsset();
                        ImGui::Text("%s Variant Count: %d", tileDef.displayName.c_str(), tileDef.modelVariants.size());
                        changed |= ImguiUtil::ObjectVector<BiomePossibleVariant>("Variants", o.variants,
                            [](BiomePossibleVariant& o, ui32) {
                            return updateAndRenderImguiControls(o);
                        });
                        // Fix up any invalid variants
                        for (auto& variant : o.variants) {
                            variant.tileVariantIndex = glm::clamp(variant.tileVariantIndex, (ui32)0, (ui32)tileDef.modelVariants.size());
                        }
                        return changed;
                    }
                });
                return changed;
            });
        }
    }

    if (changed) {
        BiomeRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }

    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();
}

void BiomeEditorViewportPanel::onEnter() {
    if (mFirstEntry) {
        resetCamera();
        mFirstEntry = false;
    }
    if (mAssetData) {
        if (!mEditorWorld) {
            initializeWorld();
        }
        else {
            updateActiveEditorWorld(mEditorWorld.get());
        }
    }
}

void BiomeEditorViewportPanel::onExit() {

    if (mEditorWorld) {
        mShuttingDownWorld = true; // Will be cleared on frame begin
        WorldDestroyer::shutdownWorld(*mEditorWorld);
        mEditorWorld.reset();
    }

    updateActiveEditorWorld(nullptr);
    {
        // Dispatch editor world
        UIContextEvent evnt;
        evnt.eventType = UIContextEventType::EditorWorldSet;
        evnt.mWorld = nullptr;
        UIContext::getInstance().dispatchEditorWorldSet(evnt);
    }
}

void BiomeEditorViewportPanel::setCurrentAsset(AssetID assetId) {
    AssetEditorViewportPanel<BiomeDef>::setCurrentAsset(assetId);
    if (mAssetData) {
        initializeWorld();
    }
}

void BiomeEditorViewportPanel::renderCenterPanel(i32AABB2* outImageRect) {
    const RenderContext& renderContext = RenderContext::getInstance();
    mActiveGBuffer = &renderContext.getActiveGBuffer();
    mActiveGBuffer->use();
    if (!mShuttingDownWorld) {
        renderContext.getWorldRenderer().renderWorld(
            renderContext.getCamera(), renderContext.getRenderData(), mActiveGBuffer, renderContext.getCurrentFrameAlpha(), renderContext.getCurrentFrameElapsedSec(), sGBuffers[0].get()
        );
    }

    vg::DepthState::NONE.set();

    if (!mShuttingDownWorld) {
        renderContext.renderPassWorldDebug(*renderContext.getCamera());
    }
    renderGrid(renderContext.getCamera()->getVPMatrix());

    vg::GBuffer::unuse();

    renderCenterPanelImage(outImageRect, sGBuffers[0]->getAlbedoTexture());

}

void BiomeEditorViewportPanel::postCenterPanelRender(const i32AABB2& imageRect)
{
    if (mWorldInterfaceController) {

        // Check if the mouse just clicked on the image
        bool clickedLeft = false;
        bool releasedLeft = false;
        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clickedLeft = true;
                mLeftMousePressed = true;
            }
            else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && mLeftMousePressed) {
                releasedLeft = true;
                mLeftMousePressed = false;
            }
        }
        else if (mLeftMousePressed) {
            releasedLeft = true;
            mLeftMousePressed = false;
        }

        // Force mouse position to the panel mouse position as we are squishing the viewport into this panel
        const f32v2 mousePos(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
        const f32v2 mouseOffset = mousePos - f32v2(imageRect.pos.x, imageRect.pos.y);
        const f32v2 mouseOffsetNormalized = mouseOffset / f32v2(imageRect.dims.x, imageRect.dims.y);
        const f32v2 viewportMousePos = mouseOffsetNormalized * f32v2(mWorldInterfaceController->getGameWindow()->getViewportDims());
        mWorldInterfaceController->setMousePosition(viewportMousePos);

        if (clickedLeft) {
            vui::InputDispatcher::injectMouseButtonEvent(viewportMousePos.x, viewportMousePos.y, vui::MouseButton::LEFT, 1, true);
        }
        else if (releasedLeft) {
            vui::InputDispatcher::injectMouseButtonEvent(viewportMousePos.x, viewportMousePos.y, vui::MouseButton::LEFT, 1, false);
        }
    }
    if (mShuttingDownWorld) {
        // We only need 1 frame to delay after world shutdown
        mShuttingDownWorld = false;
    }
}

VGTexture BiomeEditorViewportPanel::getFinalOutputTexture()
{
    if (mActiveGBuffer) {
        return sGBuffers[0]->getAlbedoTexture();
    }
    return VGTexture(0);
}

void BiomeEditorViewportPanel::initializeWorld() {
    PreciseTimer timer, timer2;
    if (mEditorWorld) {
        mShuttingDownWorld = true; // Will be cleared on frame begin
        mWorldInterfaceController.reset();
        WorldDestroyer::shutdownWorld(*mEditorWorld);
        mEditorWorld.reset();
        LOG_CRITICAL("Shutdown in {}", timer.stop());
        timer.start();
    }
    if (!mAssetData) {
        return;
    }

    LOG_INFO("Initializing Editor World...");

    // Always create new, biome grid will destroy old one
    glCreateTextures(GL_TEXTURE_2D, 1, &mBiomeTexture);
    glTextureStorage2D(mBiomeTexture, 1, GL_R8, 1, 1); // Just one pixel

    const BiomeDef& def = *mAssetData;
    const ui8 biomeId = e_cast(def.uniqueId);
    glTextureSubImage2D(mBiomeTexture, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &biomeId); // Set default biome

    HostWorldData worldData;
    worldData.worldWidth = WorldDefaults::DEFAULT_EDITOR_WORLD_WIDTH_TILES;
    worldData.playerStart = f32v2(0.5f);
    worldData.heightmapGrid = std::make_unique<HostHeightmapGrid>(worldData.worldWidth);
    worldData.roadGrid = std::make_shared<RoadGrid>(worldData.worldWidth);
    worldData.tileGrid = std::make_shared<SimChunkTileGrid>(worldData.worldWidth);
    worldData.biomeGrid = std::make_unique<BiomeGrid>(worldData.worldWidth);
    worldData.biomeGrid->setBiomeTexture(mBiomeTexture);
    for (int v = 0; v < worldData.biomeGrid->getTotalVertices(); ++v) {
        const i32v2 pos(v % worldData.biomeGrid->getWidthVertices(), v / worldData.biomeGrid->getWidthVertices());
        worldData.biomeGrid->getVertexForGenerationFromBlockPos(pos).biomeUniqueId = def.uniqueId;
    }

    LOG_CRITICAL("Set biomes {}", timer.stop()); timer.start();

    generateHeightmap(worldData);

    LOG_CRITICAL("Heightmap {}", timer.stop()); timer.start();
    //mWorldData->biomeGrid->setBiomeTexture(mWorldGenerator->releaseBiomeTexture());
    mEditorWorld = std::make_unique<World>(WorldNetMode::Editor, &worldData);

    mEditorWorld->getTimeOfDayManager().setTimeOfDay(12.0f);
    LOG_CRITICAL("Allocate world {}", timer.stop()); timer.start();

    GameThreadTasks::getInstance().addGenericTask([editorWorld = mEditorWorld.get()]() {
        editorWorld->onWorldBeginGame(f32v2(0.0f));
    });

    updateActiveEditorWorld(mEditorWorld.get());

    const f32v3 camPos = mCameraPositioner->getPosition();
    if (camPos.z < mStartHeight) {
        mCameraPositioner->setPosition(f32v3(camPos.x, camPos.y, mStartHeight));
    }

    LOG_CRITICAL("Finish {}", timer2.stop());
}

void BiomeEditorViewportPanel::generateHeightmap(HostWorldData& worldData)
{
    IHeightmapGrid* heightGrid = worldData.heightmapGrid.get();
    const ui32 totalPatches = heightGrid->getTotalPatches();
    if (!mHeightSSBO) {
        // Terrain
        assert(!mHeightSSBO);
        glCreateBuffers(1, &mHeightSSBO);
        glNamedBufferStorage(mHeightSSBO, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedHeights = (GLfloat*)glMapNamedBufferRange(mHeightSSBO, 0, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    }

    const int patchLocalGroupWidth = HEIGHTMAP_VERT_WIDTH_PER_PATCH / LOCAL_GROUP_SIZE;
    const ui32 widthPatches = worldData.heightmapGrid->getWidthPatches();

    const MaterialShaderDef* def = ResourceManager::getAssetHandle<MaterialShaderDef>(CStrToken("editor_biome_height"))->tryGetLoadedAsset();
    if (!def) {
        panic("editor_biome_height.comp was not loaded. Make sure it exists and is in assets.preload");
    }

    def->useCompute();
    glProgramUniform1f(def->mProgram.getID(), def->getUniform("unSeed"), mWorldSeed);
    glProgramUniform1i(def->mProgram.getID(), def->getUniform("unBiome"), e_cast(mAssetData->uniqueId));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mHeightSSBO);

    const f32v2 rootPos = heightGrid->getSpatialGrid2D().getWorldPosXYFromID(0);
    const i32v2 vertXY = heightGrid->getSpatialGrid2D().getGridXYFromID(0) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

    glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unPatchWorldPos"), 1, &rootPos.x);
    glProgramUniform1ui(def->mProgram.getID(), def->getUniform("unYStride"), widthPatches * (ui32)HEIGHTMAP_VERT_WIDTH_PER_PATCH);
    glProgramUniform2i(def->mProgram.getID(), def->getUniform("unVertexOffset"), vertXY.x, vertXY.y);

    // Dispatch compute
    constexpr i32 MAX_COMPUTE_SIZE = 65535; // Minimum as according to openGL spec
    if (MAX_COMPUTE_SIZE < patchLocalGroupWidth * heightGrid->getTotalPatches()) {
        panic("Heightmap gen compute attempted to dispatch {} groups, but max is {}", patchLocalGroupWidth * widthPatches, MAX_COMPUTE_SIZE);
    }
    glDispatchCompute(patchLocalGroupWidth * widthPatches, patchLocalGroupWidth * widthPatches, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    mFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFinish();

    while (glClientWaitSync(mFence, GL_SYNC_FLUSH_COMMANDS_BIT, 100) != GL_ALREADY_SIGNALED) {
        std::this_thread::yield();
    }

    // Copy data
    const int sourceYStride = HEIGHTMAP_VERT_WIDTH_PER_PATCH * widthPatches;
    for (int py = 0; py < widthPatches; ++py) {
        const int sourceYOffset = py * sourceYStride * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        for (int px = 0; px < widthPatches; ++px) {
            const int sourceXOffset = px * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            HeightmapPatch& patch = worldData.heightmapGrid->getPatchForGeneration(py * widthPatches + px);
            for (int y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
                for (int x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                    const int targetIndex = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x;
                    const int sourceIndex = sourceYOffset + y * sourceYStride + sourceXOffset + x;
                    patch.setHeightAtNoClamp(targetIndex, mMappedHeights[sourceIndex] + mHeightOffset);
                }
            }
        }
    }

    // Get middlemost vertex
    mStartHeight = mMappedHeights[(sourceYStride / 2) + (sourceYStride / 2) * sourceYStride] + 3.0f + mHeightOffset;
    LOG_DEBUG("Finished generateHeightmap");
}

void BiomeEditorViewportPanel::initializeController() {
    mWorldInterfaceController = std::make_unique<EditorWorldInterfaceController>(sApp->getWindow(), *mEditorWorld, *RenderContext::getInstance().getCameraController());
    mWorldInterfaceController->init();
}

void BiomeEditorViewportPanel::updateActiveEditorWorld(World* world) {
    GameThread::getInstance().setActiveEditorWorld(world);

    // Dispatch editor world
    UIContextEvent evnt;
    evnt.eventType = UIContextEventType::EditorWorldSet;
    evnt.mWorld = world;
    UIContext::getInstance().dispatchEditorWorldSet(evnt);

    mWorldInterfaceController.reset();
    if (world) {
        initializeController();
    }
}

void BiomeEditorViewportPanel::resetCamera() {
    f32v3 startPos = mEditorWorld->getDefaultSpawn();
    startPos.z = mStartHeight;
    mCameraPositioner->setPosition(startPos);
}
