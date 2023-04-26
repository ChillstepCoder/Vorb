#include "stdafx.h"
#include "BiomeEditorViewportPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "resources/ResourceManager.h"

#include "world/Chunk.h"

#include "camera/SimpleCamera.h"

constexpr ui32 EDITOR_CHUNK_GRID_WIDTH = 2; // nxn grid
constexpr ui32 NUM_EDITOR_CHUNKS = SQ(EDITOR_CHUNK_GRID_WIDTH);

BiomeEditorViewportPanel::BiomeEditorViewportPanel()
{

}

BiomeEditorViewportPanel::~BiomeEditorViewportPanel()
{

}

bool BiomeEditorViewportPanel::updateAndRender()
{
    if (!mChunks) {
        initializeChunks();
    }

    bool isOpen = true;
    ImGui::Begin("Material Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    updateCamera(imageDims.x / imageDims.y);

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

    renderGrid();
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

const MaterialShader* BiomeEditorViewportPanel::getShader()
{
    return nullptr;
}

void BiomeEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit)
{
    
}

void BiomeEditorViewportPanel::renderMesh()
{
    
}

void BiomeEditorViewportPanel::initializeChunks()
{
    mChunks = std::unique_ptr<Chunk[]>(new Chunk[NUM_EDITOR_CHUNKS]);
   /* for (ui32 i = 0; i < NUM_EDITOR_CHUNKS; ++i) {
        mChunks[i].init(ChunkID(i));
    }*/
}
