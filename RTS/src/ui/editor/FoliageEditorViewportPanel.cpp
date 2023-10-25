#include "stdafx.h"
#include "FoliageEditorViewportPanel.h"


#include <imgui.h>
#include <imgui_internal.h>


#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "tile/TileGrass.h"

#include "resources/TileGrassRepository.h"
#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/renderer/GrassRenderer.h"

#include "rendering/RenderContext.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/GrassBillboardMesh.h"
#include "rendering/mesh/mesher/builder/GrassMeshBuilderMethods.h"
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

void FoliageEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec)
{
    renderCenterPanel(nullptr);
}

void FoliageEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
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
    if (!mAssetData) {
        return;
    }

    if (!mGrassRenderer) {
        mGrassRenderer = std::make_unique<GrassRenderer>();
        mGrassMeshes.emplace_back(std::make_unique<GrassMesh>(0));
        mGrassMeshesSet.insert(mGrassMeshes.begin()->get());
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    // Lazy init resources
    const i32v2 imageDims = i32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateFramebufferAndLazyInit(imageDims);

    sGBuffers[0]->use();

    if (mAssetWasChanged) {
        // Refresh uniform data
        GrassRenderer::updateUniformBuffer();

        GrassBillboardMesh& mesh = mGrassMeshes.begin()->get()->mMesh;
        mesh.destroy(); // If we already had a mesh, make sure to clean up
        constexpr ui32 WIDTH_TILES = 8;
        TileGrass grassDataArray[SQ(WIDTH_TILES)];
        for (ui32 i = 0; i < SQ(WIDTH_TILES); ++i) {
            int x = i % WIDTH_TILES;
            const f32 gradientAlpha = (f32)x / (WIDTH_TILES - 1);
            const int density = round(glm::lerp((f32)mDensityGradient.x, (f32)mDensityGradient.y, gradientAlpha));
            grassDataArray[i].grassIDs[0] = mAssetData->getID();
            grassDataArray[i].densities[0] = (ui8)glm::clamp(density, 0, 255);
        }
        GrassBillboardMeshBuilder builder(mesh);
        GrassMeshBuilderMethods::editorCreateGrassMesh(builder, WIDTH_TILES, grassDataArray);
        builder.finishMesh();
        mAssetWasChanged = false;
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
    if (!mAssetData) {
        return;
    }
    ImGui::Text(mAssetData->getName().toString().c_str());
    if (ImGui::SliderFloat2("Size Mults", &mAssetData->mSizeMults.x, 0.01f, 10.0f)) {
        mAssetWasChanged = true;
    }
    if (ImGui::SliderFloat2("Size Variance", &mAssetData->mHeightVariance.x, 0.01f, 1.0f)) {
        mAssetWasChanged = true;
    }
    if (ImGui::SliderFloat2("ZOffset Variance", &mAssetData->mZOffsetVariance.x, 0.0f, 2.0f)) {
        mAssetWasChanged = true;
    }
    if (ImGui::SliderFloat("Lean Variance", &mAssetData->mLeanVariance, 0.0f, 2.0f)) {
        mAssetWasChanged = true;
    }
    if (ImGui::Checkbox("Use Gradient Color", &mAssetData->mUseGradientColor)) {
        mAssetWasChanged = true;
    }

    constexpr const char* meshShapes[e_cast(TileGrassMeshType::COUNT)] = {
      "default",
      "plane",
      "billboard",
    };
    static_assert(e_cast(TileGrassMeshType::COUNT) == 3);
    if (ImGui::BeginCombo("Mesh Shape", meshShapes[e_cast(mAssetData->mMeshType)])) {
        for (int i = 0; i < e_cast(TileGrassMeshType::COUNT); ++i) {
            bool isSelected = e_cast(mDrawMode) == i;
            if (ImGui::Selectable(meshShapes[i], &isSelected)) {
                mAssetWasChanged = true;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mAssetData->mMeshType = (TileGrassMeshType)i;
            }
        }
        ImGui::EndCombo();
    }

    // Densities are power of two but display as index
    int density = mAssetData->mDensity;
    if (ImGui::SliderInt("Density", &density, 1, MAX_GRASS_DETAIL)) {
        mAssetData->mDensity = density;
        mAssetWasChanged = true;
    }

    ImGui::Separator();
    if (ImGui::SliderInt2("Density Gradient", &mDensityGradient.x, 0, 255)) {
        mAssetWasChanged = true;
    }
}
