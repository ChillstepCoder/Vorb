#pragma once

#include <Vorb/ui/InputDispatcher.h>

#include <imgui.h>

#include "ui/editor/AssetEditorViewportPanelBase.h"

DECL_VG(class GBuffer);
class DebugTweakerPanel;
class WorldEditorPanel;
class Camera3D;
class TileEditorPanel;
class ModelDef;
struct TileGrassDef;
struct FishDef;
class ParticleSystemDef;
class ModelEditorViewportPanel;
class MaterialEditorViewportPanel;
class BiomeEditorViewportPanel;
class ContentBrowserPanel;
class FoliageEditorViewportPanel;
class FishingEditorViewportPanel;
class ParticleSystemEditorViewportPanel;
class IEditorViewportPanel;
class World;

class EditorRoot
{
public:
    EditorRoot();
    ~EditorRoot();
    void updateEditors(World* world, const Camera3D& camera, const f32v3& mousePickRay);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec);
    void renderEditorBrushDecals(const Camera3D& camera);

    bool hasActiveCenterPanel() const { return mActiveCenterPanel != nullptr; }
    IEditorViewportPanel* getActiveCenterPanel() const { return mActiveCenterPanel; }

    // Returns false if there is no valid editor
    bool tryOpenAssetForEdit(AssetDescriptor desc);
private:
    void openBiomeForEdit();
    void setActiveCenterPanel(IEditorViewportPanel* newCenterPanel);

    // Center panel display
    IEditorViewportPanel* mActiveCenterPanel = nullptr;

    // Subpanels
    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditorPanel> mWorldEditorPanel;
    std::unique_ptr<TileEditorPanel> mTileEditorPanel;
    // Viewport panels
    std::map<AssetType, std::unique_ptr<AssetEditorViewportPanelBase>> mAssetEditorPanels;

    std::unique_ptr<ContentBrowserPanel> mContentBrowserPanel;

    // Event listeners
    vui::KeyListeners mKeyListeners;
    vui::WindowListeners mWindowListeners;

    // Docking
    bool mRebuildDockspace = true;
    ImGuiID mDockspaceID;
};

