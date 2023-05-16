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
#include "rendering/UboHelpers.h"

#include "camera/Camera3D.h"
#include "camera/SimpleCamera.h"

FoliageEditorViewportPanel::FoliageEditorViewportPanel()
{
    mShowDrawModeDropdown = false;
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
    
    renderGrassControls();

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

    if (mDirtyFoliageMesh) {
        GrassBillboardMesh& mesh = mGrassMeshes.begin()->get()->mMesh;
        mesh.destroy(); // If we already had a mesh, make sure to clean up
        constexpr ui32 WIDTH_TILES = 8;
        TileGrass grassDataArray[SQ(WIDTH_TILES)];
        for (ui32 i = 0; i < SQ(WIDTH_TILES); ++i) {
            int x = i % WIDTH_TILES;
            const f32 gradientAlpha = (f32)x / (WIDTH_TILES - 1);
            const int density = round(glm::lerp((f32)mDensityGradient.x, (f32)mDensityGradient.y, gradientAlpha));
            grassDataArray[i].grassIDs[0] = mGrassData->mId;
            grassDataArray[i].densities[0] = (ui8)glm::clamp(density, 0, 255);
        }

        GrassMeshBuilder::editorCreateGrassMesh(mesh, WIDTH_TILES, grassDataArray);
        mesh.finishMesh();
        mDirtyFoliageMesh = false;
    }

    Camera3D camera3D;
    camera3D.copyFromSimpleCamera(*camera);
    
    UboHelpers::uploadCameraUbo(RenderContext::getInstance().getCameraUbo(), camera3D);
    if (mGrassMeshes.begin()->get()->mMesh.isValid()) {
        mGrassRenderer->renderGrass(camera3D, f32v3(FLT_MAX), mGrassMeshesSet);
    }

    vg::DepthState::NONE.set();
    renderGrid(camera->getViewProjectionMatrix());

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    renderSkybox();

    vg::GBuffer::unuse();

    renderCenterPanelImage(outImageRect, getFinalOutputTexture());
}

void FoliageEditorViewportPanel::renderGrassControls()
{
    if (!mGrassData) {
        return;
    }
    ImGui::Text(mGrassData->mName.toString().c_str());
    if (ImGui::SliderFloat2("Size Mults", &mGrassData->mSizeMults.x, 0.01f, 10.0f)) {
        mDirtyFoliageMesh = true;
    }
    if (ImGui::SliderFloat2("Height Variance", &mGrassData->mHeightVariance.x, 0.01f, 1.0f)) {
        mDirtyFoliageMesh = true;
    }
    if (ImGui::SliderFloat("Lean Variance", &mGrassData->mLeanVariance, 0.0f, 2.0f)) {
        mDirtyFoliageMesh = true;
    }
    if (ImGui::Checkbox("Use Gradient Color", &mGrassData->mUseGradientColor)) {
        mDirtyFoliageMesh = true;
    }

    constexpr const char* meshShapes[e_cast(TileGrassMeshType::COUNT)] = {
      "default",
      "plane",
    };
    static_assert(e_cast(TileGrassMeshType::COUNT) == 2);
    if (ImGui::BeginCombo("Mesh Shape", meshShapes[e_cast(mGrassData->mMeshType)])) {
        for (int i = 0; i < e_cast(TileGrassMeshType::COUNT); ++i) {
            bool isSelected = e_cast(mDrawMode) == i;
            ImGui::Selectable(meshShapes[i], &isSelected);

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mGrassData->mMeshType = (TileGrassMeshType)i;
            }
        }
        ImGui::EndCombo();
    }

    // Densities are power of two but display as index
    int density = mGrassData->mDensity;
    if (ImGui::SliderInt("Density", &density, 1, MAX_GRASS_DETAIL)) {
        mGrassData->mDensity = density;
        mDirtyFoliageMesh = true;
    }

    ImGui::Separator();
    if (ImGui::SliderInt2("Density Gradient", &mDensityGradient.x, 0, 255)) {
        mDirtyFoliageMesh = true;
    }
}
