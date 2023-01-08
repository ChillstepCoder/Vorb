#pragma once

#include <Vorb/ui/InputDispatcher.h>

DECL_VG(class GBuffer);
class DebugTweakerPanel;
class WorldEditorPanel;
class Camera3D;
class TileEditorPanel;
struct ModelDef;
struct MaterialHandle;
class ModelEditorPanel;
class MaterialEditorPanel;
class IEditorViewportPanel;

class EditorRoot
{
public:
    EditorRoot();
    ~EditorRoot();
    void updateEditors(const Camera3D& camera, const f32v3& mousePickRay);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer);
    void renderEditorBrushDecals(const Camera3D& camera);

private:
    void openModelForEdit(ModelDef& model);
    void openMaterialForEdit(MaterialHandle& materialHandle);

    // Center panel display
    IEditorViewportPanel* mActiveCenterPanel = nullptr;

    // Subpanels
    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditorPanel> mWorldEditorPanel;
    std::unique_ptr<TileEditorPanel> mTileEditorPanel;
    // Center panels
    std::unique_ptr<ModelEditorPanel> mModelEditorPanel;
    std::unique_ptr<MaterialEditorPanel> mMaterialEditorPanel;

    // Event listeners
    vui::KeyListeners mKeyListeners;
    vui::WindowListeners mWindowListeners;
};

