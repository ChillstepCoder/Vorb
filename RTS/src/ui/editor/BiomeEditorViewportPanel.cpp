#include "stdafx.h"
#include "BiomeEditorViewportPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

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

constexpr ui32 EDITOR_CHUNK_GRID_WIDTH = 2; // nxn grid
constexpr ui32 NUM_EDITOR_CHUNKS = SQ(EDITOR_CHUNK_GRID_WIDTH);

BiomeEditorViewportPanel::BiomeEditorViewportPanel() {

}

BiomeEditorViewportPanel::~BiomeEditorViewportPanel()
{

}

bool BiomeEditorViewportPanel::updateAndRender()
{
   
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

    renderCenterPanel();

    ImGui::End();

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
}

void BiomeEditorViewportPanel::onExit() {
    GameThreadTasks::getInstance().setActiveEditorWorld(nullptr);
}

void BiomeEditorViewportPanel::renderCenterPanel() {
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
