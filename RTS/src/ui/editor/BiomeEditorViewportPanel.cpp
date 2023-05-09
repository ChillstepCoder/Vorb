#include "stdafx.h"
#include "BiomeEditorViewportPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "world/controller/EditorWorldInterfaceController.h"

#include "resources/ResourceManager.h"

#include "world/IWorld.h"
#include "world/Chunk.h"
#include "world/WorldFactory.h"
#include "rendering/renderer/WorldRenderer.h"

#include "rendering/RenderContext.h"

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

bool BiomeEditorViewportPanel::updateAndRender() {
   
    bool isOpen = true;
    ImGui::Begin("Biome Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    updateCamera(imageDims.x / imageDims.y);


    // Tell the world to follow our camera
    if (mEditorWorld) {
        const f32v2 cameraPos = camera->getPosition();
        IWorld* editorWorld = mEditorWorld.get();
        GameThreadTasks::getInstance().addGenericTaskWithCapture([cameraPos, editorWorld](GameThread&, void* vWorld) {
            //assert(editorWorld == static_cast<IWorld*>(vWorld));
            editorWorld->setLoadCenter(cameraPos);
        }, (void*)editorWorld);
    }

    //if (mCurrentMaterial.isValid()) {
    //    ImGui::Text(mCurrentMaterial.name.c_str());
    //}
    //else {
    //    ImGui::Text("NO MATERIAL");
    //}

    // Lazy init so we don't use GPU memory when not in editor
    if (sGBuffers[0] == nullptr) {
        initGBuffers(imageDims);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    i32AABB2 imageRect;
    renderCenterPanel(&imageRect);

    // Check if the mouse just clicked on the image
    bool clickedLeft = false;
    bool releasedLeft = false;
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            clickedLeft = true;
            mLeftMousePressed = true;
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            releasedLeft = true;
            mLeftMousePressed = false;
        }
    }
    else if (mLeftMousePressed) {
        releasedLeft = true;
        mLeftMousePressed = false;
    }
    ImGui::End();

    if (mWorldInterfaceController) {
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

        mWorldInterfaceController->update();
        mWorldInterfaceController->renderUI();
    }

    return isOpen;
}

void BiomeEditorViewportPanel::updateAndRenderControls(f32 ySize) {
    ImGui::BeginChild("Biome Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Biome Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    ImGui::EndChild();
}

void BiomeEditorViewportPanel::onEnter() {
    if (!mEditorWorld) {
        initializeWorld();
        positioner->setPosition(mEditorWorld->getDefaultSpawn());
    }

    GameThreadTasks::getInstance().setActiveEditorWorld(mEditorWorld.get());
    {
        // Dispatch editor world
        UIContextEvent evnt;
        evnt.eventType = UIContextEventType::EditorWorldSet;
        evnt.mWorld = mEditorWorld.get();
        UIContext::getInstance().dispatchEditorWorldSet(evnt);
    }

    mWorldInterfaceController = std::make_unique<EditorWorldInterfaceController>(sApp->getWindow(), *mEditorWorld, *RenderContext::getInstance().getCameraController());
    mWorldInterfaceController->init();
}

void BiomeEditorViewportPanel::onExit() {
    GameThreadTasks::getInstance().setActiveEditorWorld(nullptr);
    {
        // Dispatch editor world
        UIContextEvent evnt;
        evnt.eventType = UIContextEventType::EditorWorldSet;
        evnt.mWorld = nullptr;
        UIContext::getInstance().dispatchEditorWorldSet(evnt);
    }

    mWorldInterfaceController.reset();
}

void BiomeEditorViewportPanel::renderCenterPanel(i32AABB2* outImageRect) {
    const RenderContext& renderContext = RenderContext::getInstance();
    mActiveGBuffer = &renderContext.getActiveGBuffer();
    mActiveGBuffer->use();
    renderContext.getWorldRenderer().renderWorld(
        renderContext.getCamera(), renderContext.getRenderData(), mActiveGBuffer, renderContext.getCurrentFrameAlpha(), renderContext.getCurrentFrameElapsedSec(), sGBuffers[0].get()
    );

    vg::DepthState::NONE.set();

    renderContext.renderPassWorldDebug(*renderContext.getCamera());
    if (mRenderGrid) {
        renderGrid(renderContext.getCamera()->getVPMatrix());
    }

    vg::GBuffer::unuse();

    VGTexture displayTexture = sGBuffers[0]->getAlbedoTexture();
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)displayTexture, dims, uv0, uv1);

    if (outImageRect) {
        ImVec2 imageRectMin = ImGui::GetItemRectMin();
        outImageRect->dims = imageDims;
        outImageRect->pos = f32v2(imageRectMin.x, imageRectMin.y);
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
    LOG_INFO("Initializing Editor World...");
    mEditorWorld = WorldFactory::makeWorld(WorldNetMode::Editor, WorldData::DEFAULT_EDITOR_WORLD_WIDTH_TILES);

    GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vWorld) {
        IWorld* editorWorld = static_cast<IWorld*>(vWorld);
        editorWorld->getTimeOfDayManager().setTimeOfDay(12.0f);
        editorWorld->onWorldBegin(f32v2(0.0f));
    }, (void*)mEditorWorld.get());
}
