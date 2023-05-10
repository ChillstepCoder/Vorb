#include "stdafx.h"
#include "FoliageEditorViewportPanel.h"


#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "tile/TileGrass.h"

#include "resources/TileGrassRepository.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/renderer/GrassRenderer.h"

#include "rendering/RenderContext.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/GrassBillboardMesh.h"
#include "rendering/mesh/mesher/builder/GrassMeshBuilder.h"

#include "camera/Camera3D.h"
#include "camera/SimpleCamera.h"

FoliageEditorViewportPanel::FoliageEditorViewportPanel()
{

}

FoliageEditorViewportPanel::~FoliageEditorViewportPanel()
{

}

bool FoliageEditorViewportPanel::updateAndRender()
{
    if (!mGrassRenderer) {
        mGrassRenderer = std::make_unique<GrassRenderer>();
        mGrassMeshes.emplace_back(std::make_unique<GrassMesh>(0));
        mGrassMeshesSet.insert(mGrassMeshes.begin()->get());
    }

    bool isOpen = true;
    ImGui::Begin("Foliage Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    updateCamera(imageDims.x / imageDims.y);

    updateFramebufferAndLazyInit(imageDims);

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    clearFramebuffers();
    
    renderCenterPanel(nullptr);

    ImGui::End();

    return isOpen;
}

void FoliageEditorViewportPanel::updateAndRenderControls(f32 ySize)
{
    ImGui::BeginChild("Foliage Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Foliage Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();
    

    ImGui::EndChild();
}

void FoliageEditorViewportPanel::renderCenterPanel(i32AABB2* outImageRect) {
    if (!mGrassData) {
        return;
    }
    // Lazy init resources
    const i32v2 imageDims = i32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateFramebufferAndLazyInit(imageDims);

    sGBuffers[0]->use();

    GrassBillboardMesh& mesh = mGrassMeshes.begin()->get()->mMesh;
    constexpr ui32 WIDTH_TILES = 8;
    TileGrass grassDataArray[SQ(WIDTH_TILES)];
    for (ui32 i = 0; i < SQ(WIDTH_TILES); ++i) {
        grassDataArray[i].grassIDs[0] = mGrassData->mId;
        grassDataArray[i].densities[0] = 255;
    }

    GrassMeshBuilder::editorCreateGrassMesh(mesh, WIDTH_TILES, grassDataArray);
    mesh.finishMesh();

    Camera3D camera3D;
    camera3D.copyFromSimpleCamera(*camera);
    mGrassRenderer->renderGrass(camera3D, f32v3(FLT_MAX), mGrassMeshesSet);
    mesh.destroy();

    vg::DepthState::NONE.set();
    renderGrid(camera->getViewProjectionMatrix());

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    renderSkybox();

    vg::GBuffer::unuse();

    renderCenterPanelImage(outImageRect, getFinalOutputTexture());
}
