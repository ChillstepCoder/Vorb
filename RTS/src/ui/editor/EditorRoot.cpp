#include "stdafx.h"
#include "EditorRoot.h"

#include "editor/WorldEditorPanel.h"
#include "ui/DebugTweakerPanel.h"
#include "ui/editor/AssetSelectPanel.h"
#include "ui/editor/ModelEditorViewportPanel.h"
#include "ui/editor/MaterialEditorViewportPanel.h"
#include "ui/editor/BiomeEditorViewportPanel.h"
#include "ui/editor/FoliageEditorViewportPanel.h"
#include "ui/editor/FishingEditorViewportPanel.h"
#include "ui/editor/ParticleSystemEditorViewportPanel.h"
#include "ui/editor/ItemEditorViewportPanel.h"
#include "ui/editor/TileEditorViewportPanel.h"
#include "ui/editor/TileDistributionEditorViewportPanel.h"
#include "ui/editor/ContentBrowserPanel.h"
#include "options/DebugOptions.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "ui/ImguiUtil.hpp"


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
    mTileEditorPanel = std::make_unique<AssetSelectPanel>();

    // Asset editors
    mAssetEditorPanels[AssetType::Model] = std::make_unique<ModelEditorViewportPanel>();
    mAssetEditorPanels[AssetType::Material] = std::make_unique<MaterialEditorViewportPanel>();
    mAssetEditorPanels[AssetType::TileGrass] = std::make_unique<FoliageEditorViewportPanel>();
    mAssetEditorPanels[AssetType::Fish] = std::make_unique<FishingEditorViewportPanel>();
    mAssetEditorPanels[AssetType::ParticleSystem] = std::make_unique<ParticleSystemEditorViewportPanel>();
    mAssetEditorPanels[AssetType::Item] = std::make_unique<ItemEditorViewportPanel>();
    mAssetEditorPanels[AssetType::Biome] = std::make_unique<BiomeEditorViewportPanel>();
    mAssetEditorPanels[AssetType::Tile] = std::make_unique<TileEditorViewportPanel>();
    mAssetEditorPanels[AssetType::TileDistribution] = std::make_unique<TileDistributionEditorViewportPanel>();

    mContentBrowserPanel = std::make_unique<ContentBrowserPanel>(ResourceManager::get().getResourceRoot().getStdPath());

    // Initialize inputs
    vui::InputDispatcher::key.registerKeyListeners(mKeyListeners);
    vui::InputDispatcher::window.registerWindowListeners(mWindowListeners);

    vui::InputDispatcher::key.addKeyDownListener(mKeyListeners, [this](const vui::KeyEvent& event) {
        if (event.keyCode == VKEY_5) {
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

void EditorRoot::updateEditors(World* world, const Camera3D& camera, const f32v3& mousePickRay) {
    if (sDebugOptions.mShowEditor) {
        mWorldEditorPanel->update(world, camera, mousePickRay);
    }
}

void EditorRoot::updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec) {
    if (sDebugOptions.mShowEditor) {

        { // Dockspace
//             ImguiUtil::ScopedColorStack dockspaceColors(
//                 ImGuiCol_TitleBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
//                 ImGuiCol_TitleBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
//                 ImGuiCol_TitleBgCollapsed, ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
//                 ImGuiCol_TabActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), // NOTE(Peter): Disable tab bar underline
//                 ImGuiCol_TabUnfocusedActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f) // NOTE(Peter): Disable tab bar underline
//             );
//             //ImGui::DockSpace(mDockspaceID, ImVec2(0, 0)/*ImGui::GetContentRegionAvail()*/, ImGuiDockNodeFlags_PassthruCentralNode);//)ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);
//             mDockspaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);
        }

        ImGuiViewport* viewport;
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        {
            viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // https://gist.github.com/PossiblyAShrub/0aea9511b84c34e191eaa90dd7225969#file-dock_builder_example-cpp-L8
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);
        ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags);
        //ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);

        // Rebuild Dockspace
        
        if (mRebuildDockspace)
        {
            mRebuildDockspace = false;
            ImGuiID rootId = dockspaceId;
            ImGui::DockBuilderRemoveNode(rootId); // clear any previous layout
            ImGui::DockBuilderAddNode(rootId, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(rootId, viewport->Size);

            //ImGuiID dockIdUp;
            auto dockIdLeft = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, nullptr, &dockspaceId);
            auto dockIdRight = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &dockspaceId);
            auto dockIdDown = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.25f, nullptr, &dockspaceId);

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("Primary Controls", dockIdLeft);
            ImGui::DockBuilderDockWindow("Secondary Controls", dockIdRight);
            ImGui::DockBuilderDockWindow("Content Browser", dockIdDown);
            ImGui::DockBuilderDockWindow("Bottom Controls", dockIdDown);
            ImGui::DockBuilderDockWindow("Biome  Editor", dockspaceId);
            for (auto&& it : mAssetEditorPanels) {
                ImGui::DockBuilderDockWindow(it.second->getViewportWindowName(), dockspaceId);
            }
            ImGui::DockBuilderFinish(rootId);
        }

        ImGui::End(); // End dockspace

        World* world = mWorldEditorPanel->getActiveWorld();
        const ui32v2& dims = vui::InputDispatcher::window.getCurrentWindowDims();
        const f32 defaultPanelWidth = dims.x * 0.16f;
        const ImGuiCond cond = ImGuiCond_Once /*ImGuiCond_FirstUseEver*/;
        f32 leftPanelWidth = 0.0f;
        f32 rightPanelWidth = 0.0f;

        { // Left Panel
            //ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), cond);
            ImGui::SetNextWindowSize(ImVec2(defaultPanelWidth, dims.y), cond);
            // ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus)
            //ImGui::Begin("pl", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar/* | ImGuiWindowFlags_NoMove*/);
            ImGui::Begin("Primary Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus);
            leftPanelWidth = ImGui::GetCurrentWindowRead()->Size.x;

            static f32 ySize1 = ImGui::GetContentRegionAvail().y * 0.5f;
            static f32 ySize2 = ySize1;
            Splitter(false, 10.0f, &ySize1, &ySize2, 8, 8, ImGui::GetContentRegionAvail().x);

            // If active center panel wants to render over world editor UI, let it
            if (!mActiveCenterPanel || !mActiveCenterPanel->updateAndRenderSecondaryControls(ySize1)) {
                mWorldEditorPanel->renderUI(ySize1);
            }

            // If active center panel wants to render over tweaker UI, let it
            if (!mActiveCenterPanel || !mActiveCenterPanel->updateAndRenderTertiaryControls(ySize1)) {
                if (world) {
                    mDebugTweakerPanel->updateAndRender(*world, activeGBuffer, ySize2, vui::InputDispatcher::window.getCurrentAspectRatio());
                }
            }

            ImGui::End();
        }

        { // Right Panel
            //ImGui::SetNextWindowPos(ImVec2(dims.x - defaultPanelWidth, 0.0f), cond);
            ImGui::SetNextWindowSize(ImVec2(defaultPanelWidth, dims.y), cond);
            ImGui::Begin("Secondary Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus);
            rightPanelWidth = ImGui::GetCurrentWindowRead()->Size.x;

            AssetSelectPanelResult result;
            static f32 ySize1 = ImGui::GetContentRegionAvail().y * 0.5f;
            static f32 ySize2 = ySize1;
            // With model editor open we render controls panel
            if (mActiveCenterPanel) {
                Splitter(false, 10.0f, &ySize1, &ySize2, 8, 8, ImGui::GetContentRegionAvail().x);
                result = mTileEditorPanel->updateAndRender(ySize1);

                // Controls
                mActiveCenterPanel->updateAndRenderPrimaryControls(ySize2);
            }
            else {
                result = mTileEditorPanel->updateAndRender(ImGui::GetContentRegionAvail().y);
            }

            ImGui::End();

            switch (result.first)
            {
                case AssetSelectPanelResultCode::NONE:
                    break;
                case AssetSelectPanelResultCode::EDIT_ASSET: {
                    tryOpenAssetForEdit(result.second);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            static_assert(e_count(AssetSelectPanelResultCode) == 2);

        }
        if (mActiveCenterPanel) {
            // Optional bottom panel
            if (mActiveCenterPanel->hasBottomControls()) {
                mActiveCenterPanel->updateAndRenderBottomControls();
            }
            // Center panel
            if (!mActiveCenterPanel->updateAndRender(elapsedSec)) {
                setActiveCenterPanel(nullptr);
            }
        }

        // Content browser
        mContentBrowserPanel->updateAndRender(elapsedSec, nullptr);

    }
}

void EditorRoot::renderEditorBrushDecals(const Camera3D& camera) {
    if (sDebugOptions.mShowEditor) {
        mWorldEditorPanel->renderBrushDecals(camera);
    }
}

bool EditorRoot::tryOpenAssetForEdit(AssetDescriptor desc)
{
    auto&& it = mAssetEditorPanels.find(desc.assetType);
    if (it == mAssetEditorPanels.end()) {
        return false;
    }
    it->second->setCurrentAsset(desc.id);
    setActiveCenterPanel(it->second.get());
    return true;
}

void EditorRoot::setActiveCenterPanel(IEditorViewportPanel* newCenterPanel)
{
    if (mActiveCenterPanel != newCenterPanel) {
        if (mActiveCenterPanel) mActiveCenterPanel->onExit();
        mActiveCenterPanel = newCenterPanel;
        if (mActiveCenterPanel) mActiveCenterPanel->onEnter();
    }
}
