#include "stdafx.h"
#include "BiomeEditorViewportPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "world/controller/EditorWorldInterfaceController.h"

#include "resources/ResourceManager.h"

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

constexpr ui32 EDITOR_CHUNK_GRID_WIDTH = 2; // nxn grid
constexpr ui32 NUM_EDITOR_CHUNKS = SQ(EDITOR_CHUNK_GRID_WIDTH);

BiomeEditorViewportPanel::BiomeEditorViewportPanel() {

}

BiomeEditorViewportPanel::~BiomeEditorViewportPanel()
{

}

void BiomeEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
   
    // Tell the world to follow our camera
    if (mEditorWorld) {
        const f32v2 cameraPos = mCamera->getPosition();
        World* editorWorld = mEditorWorld.get();
        GameThreadTasks::getInstance().addGenericTaskWithCapture([cameraPos, editorWorld](GameThread&, void* vWorld) {
            //assert(editorWorld == static_cast<IWorld*>(vWorld));
            editorWorld->setLoadCenter(cameraPos);
        }, (void*)editorWorld);
    }
}

void BiomeEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Biome Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Biome Editor Controls");

    if (ImguiUtil::updateAndRenderSoftAssetReference("Biome", mSelectedBiome)) {
        initializeWorld();
    }
    bool changed = false;
    if (mAssetData) {
        ImGui::Text("Biome: %s", mAssetData->displayName.c_str());
        updateAndRenderSaveButton();
        changed |= updateAndRenderImguiControls(*mAssetData);
        // Tile gen categories
        changed |= ImguiUtil::ObjectVector<BiomeTileGenCategory>("Categories", mAssetData->tileGenCategories,
            [](BiomeTileGenCategory& o) {
                bool changed = false;
                changed |= updateAndRenderImguiControls(o);
                changed |= ImguiUtil::ObjectVector<BiomePossibleTile>("Tiles", o.tiles,
                    [](BiomePossibleTile& o) {
                        bool changed = false;
                        changed |= updateAndRenderImguiControls(o);
                        return changed;
                    }
                );
                return changed;
            }
        );
    }

    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();
}

void BiomeEditorViewportPanel::onEnter() {

    if (mAssetData) {
        mSelectedBiome.name = mAssetData->getName();

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
        mSelectedBiome.name = mAssetData->getName();
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

    if (mEditorWorld) {
        mShuttingDownWorld = true; // Will be cleared on frame begin
        mWorldInterfaceController.reset();
        WorldDestroyer::shutdownWorld(*mEditorWorld);
        mEditorWorld.reset();
    }

    LOG_INFO("Initializing Editor World...");

    // Always create new, biome grid will destroy old one
    glCreateTextures(GL_TEXTURE_2D, 1, &mBiomeTexture);
    glTextureStorage2D(mBiomeTexture, 1, GL_R8, 1, 1); // Just one pixel

    const BiomeDef& def = ResourceManager::getAssetHandle<BiomeDef>(mSelectedBiome.name)->getLoadedAsset();
    const ui8 biomeId = e_cast(def.uniqueId);
    glTextureSubImage2D(mBiomeTexture, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &biomeId); // Set default biome

    HostWorldData worldData;
    worldData.worldWidth = WorldDefaults::DEFAULT_EDITOR_WORLD_WIDTH_TILES;
    worldData.playerStart = f32v2(0.5f);
    worldData.heightmapGrid = std::make_unique<HostHeightmapGrid>(worldData.worldWidth);
    worldData.biomeGrid = std::make_unique<BiomeGrid>(worldData.worldWidth);
    worldData.biomeGrid->setBiomeTexture(mBiomeTexture);
    for (int v = 0; v < worldData.biomeGrid->getTotalVertices(); ++v) {
        worldData.biomeGrid->getVertexForGeneration(v).biomeUniqueId = def.uniqueId;
    }

    // TODO: Replace with proper GPU gen
    for (int v = 0; v < worldData.heightmapGrid->getTotalPatches(); ++v) {
        HeightmapPatch& patch = worldData.heightmapGrid->getPatchForGeneration(v);
        for (int i = 0; i < HEIGHTMAP_VERT_SIZE_PER_PATCH; ++i) {
            patch.setHeightAtNoClamp(i, 1.0f);
        }
    }

    //mWorldData->biomeGrid->setBiomeTexture(mWorldGenerator->releaseBiomeTexture());
    mEditorWorld = std::make_unique<World>(WorldNetMode::Editor, &worldData);

    mEditorWorld->getTimeOfDayManager().setTimeOfDay(12.0f);
    mEditorWorld->onWorldBegin(f32v2(0.0f));
    updateActiveEditorWorld(mEditorWorld.get());

    mCameraPositioner->setPosition(mEditorWorld->getDefaultSpawn());
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
