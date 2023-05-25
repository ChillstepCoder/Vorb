#pragma once

#include <Vorb/ui/InputDispatcher.h>

DECL_VG(class GBuffer);
class DebugTweakerPanel;
class WorldEditorPanel;
class Camera3D;
class TileEditorPanel;
class ModelDef;
struct MaterialHandle;
struct TileGrassData;
class ModelEditorViewportPanel;
class MaterialEditorViewportPanel;
class BiomeEditorViewportPanel;
class FoliageEditorViewportPanel;
class IEditorViewportPanel;
class IWorld;

class EditorRoot
{
public:
    EditorRoot();
    ~EditorRoot();
    void updateEditors(IWorld* world, const Camera3D& camera, const f32v3& mousePickRay);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer);
    void renderEditorBrushDecals(const Camera3D& camera);

    bool hasActiveCenterPanel() const { return mActiveCenterPanel != nullptr; }
    IEditorViewportPanel* getActiveCenterPanel() const { return mActiveCenterPanel; }

private:
    void openModelForEdit(ModelDef& model);
    void openMaterialForEdit(MaterialHandle& materialHandle);
    void openFoliageForEdit(TileGrassData& grassData);
    void openBiomeForEdit();
    void setActiveCenterPanel(IEditorViewportPanel* newCenterPanel);

    // Center panel display
    IEditorViewportPanel* mActiveCenterPanel = nullptr;

    // Subpanels
    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditorPanel> mWorldEditorPanel;
    std::unique_ptr<TileEditorPanel> mTileEditorPanel;
    // Viewport panels
    std::unique_ptr<ModelEditorViewportPanel> mModelEditorViewportPanel;
    std::unique_ptr<MaterialEditorViewportPanel> mMaterialEditorViewportPanel;
    std::unique_ptr<FoliageEditorViewportPanel> mFoliageEditorViewportPanel;
    std::unique_ptr<BiomeEditorViewportPanel> mBiomeEditorViewportPanel;

    // Event listeners
    vui::KeyListeners mKeyListeners;
    vui::WindowListeners mWindowListeners;
};

