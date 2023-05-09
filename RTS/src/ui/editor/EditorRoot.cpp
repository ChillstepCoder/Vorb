#include "stdafx.h"
#include "EditorRoot.h"

#include "editor/WorldEditorPanel.h"
#include "ui/DebugTweakerPanel.h"
#include "ui/editor/TileEditorPanel.h"
#include "ui/editor/ModelEditorViewportPanel.h"
#include "ui/editor/MaterialEditorViewportPanel.h"
#include "ui/editor/BiomeEditorViewportPanel.h"
#include "options/DebugOptions.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/imgui_internal.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

// Panel splitter https://github.com/ocornut/imgui/issues/319
bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f) {
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos;
    // For some reason operator + doesn't work for ImVec2... It exists in imgui_internal
    ImVec2 minAdd = (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Min.x += minAdd.x;
    bb.Min.y += minAdd.y;
    ImVec2 maxAdd = CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    bb.Max.x = bb.Min.x + maxAdd.x;
    bb.Max.y = bb.Min.y + maxAdd.y;
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

EditorRoot::EditorRoot() {
    // Initialize panels
    mDebugTweakerPanel = std::make_unique<DebugTweakerPanel>();
    mWorldEditorPanel = std::make_unique<WorldEditorPanel>();
    mTileEditorPanel = std::make_unique<TileEditorPanel>();
    mModelEditorViewportPanel = std::make_unique<ModelEditorViewportPanel>();
    mMaterialEditorViewportPanel = std::make_unique<MaterialEditorViewportPanel>();
    mBiomeEditorViewportPanel = std::make_unique<BiomeEditorViewportPanel>();

    // Initialize inputs
    vui::InputDispatcher::key.registerKeyListeners(mKeyListeners);
    vui::InputDispatcher::window.registerWindowListeners(mWindowListeners);

    vui::InputDispatcher::key.addKeyDownListener(mKeyListeners, [this](const vui::KeyEvent& event) {
        if (event.keyCode == VKEY_T) {
            sDebugOptions.mShowEditor = !sDebugOptions.mShowEditor;
            // Notify panels that we are losing or gaining context
            if (mActiveCenterPanel) {
                if (sDebugOptions.mShowEditor) {
                    mActiveCenterPanel->onEnter();
                }
                else {
                    mActiveCenterPanel->onExit();
                }
            }
        }
    });
    vui::InputDispatcher::window.addResizeListener(mWindowListeners, [this](const vui::WindowResizeEvent& event) {
        LOG_DEBUG("Resize event {} {}", event.w, event.h);
    });
}

EditorRoot::~EditorRoot() {

}

void EditorRoot::updateEditors(IWorld* world, const Camera3D& camera, const f32v3& mousePickRay) {
    if (sDebugOptions.mShowEditor) {
        mWorldEditorPanel->update(world, camera, mousePickRay);
    }
}

void EditorRoot::updateAndRenderUI(const vg::GBuffer* activeGBuffer) {
    if (sDebugOptions.mShowEditor) {
        IWorld* world = mWorldEditorPanel->getActiveWorld();
        const ui32v2& dims = vui::InputDispatcher::window.getCurrentWindowDims();
        const f32 defaultPanelWidth = dims.x * 0.16f;
        const ImGuiCond cond = ImGuiCond_Once /*ImGuiCond_FirstUseEver*/;
        f32 leftPanelWidth = 0.0f;
        f32 rightPanelWidth = 0.0f;

        { // Left Panel
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), cond);
            ImGui::SetNextWindowSize(ImVec2(defaultPanelWidth, dims.y), cond);
            ImGui::Begin("pl", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
            leftPanelWidth = ImGui::GetCurrentWindowRead()->Size.x;

            static f32 ySize1 = ImGui::GetContentRegionAvail().y * 0.5f;
            static f32 ySize2 = ySize1;
            Splitter(false, 10.0f, &ySize1, &ySize2, 8, 8, ImGui::GetContentRegionAvail().x);

            mWorldEditorPanel->renderUI(ySize1);
            if (world) {
                mDebugTweakerPanel->updateAndRender(*world, activeGBuffer, ySize2, vui::InputDispatcher::window.getCurrentAspectRatio());
            }

            ImGui::End();
        }

        { // Right Panel
            ImGui::SetNextWindowPos(ImVec2(dims.x - defaultPanelWidth, 0.0f), cond);
            ImGui::SetNextWindowSize(ImVec2(defaultPanelWidth, dims.y), cond);
            ImGui::Begin("pr", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
            rightPanelWidth = ImGui::GetCurrentWindowRead()->Size.x;

            TileEditorPanelResult result;
            static f32 ySize1 = ImGui::GetContentRegionAvail().y * 0.5f;
            static f32 ySize2 = ySize1;
            // With model editor open we render controls panel
            if (mActiveCenterPanel) {
                Splitter(false, 10.0f, &ySize1, &ySize2, 8, 8, ImGui::GetContentRegionAvail().x);
                result = mTileEditorPanel->updateAndRender(ySize1);

                // Controls
                mActiveCenterPanel->updateAndRenderControls(ySize2);
            }
            else {
                result = mTileEditorPanel->updateAndRender(ImGui::GetContentRegionAvail().y);
            }

            ImGui::End();

            switch (result.first)
            {
                case TileEditorPanelResultCode::NONE:
                    break;
                case TileEditorPanelResultCode::EDIT_MODEL:
                    openModelForEdit(*std::get<ModelDef*>(result.second));
                    break;
                case TileEditorPanelResultCode::EDIT_MATERIAL:
                    openMaterialForEdit(*std::get<std::unique_ptr<MaterialHandle>>(result.second));
                    break;
                case TileEditorPanelResultCode::EDIT_FOLIAGE:
                    openFoliageForEdit(*std::get<TileGrassData*>(result.second));
                    break;
                default:
                    assert(false);
                    break;
            }
            static_assert(e_cast(TileEditorPanelResultCode::COUNT) == 4);

        }

        // Center panel
        if (mActiveCenterPanel) {
            f32 width = dims.x - (rightPanelWidth + leftPanelWidth);
            if (width >= 2.0f) {
                ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(width, dims.y));
                if (!mActiveCenterPanel->updateAndRender()) {
                    setActiveCenterPanel(nullptr);
                }
            }
        }
    }
}

void EditorRoot::renderEditorBrushDecals(const Camera3D& camera) {
    if (sDebugOptions.mShowEditor) {
        mWorldEditorPanel->renderBrushDecals(camera);
    }
}

void EditorRoot::openModelForEdit(ModelDef& model) {
    mModelEditorViewportPanel->setModel(model);
    setActiveCenterPanel(mModelEditorViewportPanel.get());
}

void EditorRoot::openMaterialForEdit(MaterialHandle& materialHandle) {
    mMaterialEditorViewportPanel->setMaterial(materialHandle);
    setActiveCenterPanel(mMaterialEditorViewportPanel.get());
}

void EditorRoot::openFoliageForEdit(TileGrassData& grassData)
{
    //mBiomeEditorViewportPanel->setMaterial(materialHandle);
    setActiveCenterPanel(mBiomeEditorViewportPanel.get());
}

void EditorRoot::setActiveCenterPanel(IEditorViewportPanel* newCenterPanel)
{
    if (mActiveCenterPanel != newCenterPanel) {
        if (mActiveCenterPanel) mActiveCenterPanel->onExit();
        mActiveCenterPanel = newCenterPanel;
        if (mActiveCenterPanel) mActiveCenterPanel->onEnter();
    }
}
