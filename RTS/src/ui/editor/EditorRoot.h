#pragma once

#include <Vorb/ui/InputDispatcher.h>

DECL_VG(class GBuffer);
class DebugTweakerPanel;
class WorldEditorPanel;
class Camera3D;
class TileEditorPanel;
struct ModelDef;
class ModelEditorPanel;

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

    // Center panel display
    bool mShowModelEditor = false;

    // Subpanels
    std::unique_ptr<DebugTweakerPanel> mDebugTweakerPanel;
    std::unique_ptr<WorldEditorPanel> mWorldEditorPanel;
    std::unique_ptr<TileEditorPanel> mTileEditorPanel;
    std::unique_ptr<ModelEditorPanel> mModelEditorPanel;

    // Event listeners
    vui::KeyListeners mKeyListeners;
    vui::WindowListeners mWindowListeners;
};

