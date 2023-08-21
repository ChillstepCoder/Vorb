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
struct FishDef;
class ParticleSystemDef;
class ModelEditorViewportPanel;
class MaterialEditorViewportPanel;
class BiomeEditorViewportPanel;
class FoliageEditorViewportPanel;
class FishingEditorViewportPanel;
class ParticleSystemEditorViewportPanel;
class IEditorViewportPanel;
class IWorld;

class EditorRoot
{
public:
    EditorRoot();
    ~EditorRoot();
    void updateEditors(IWorld* world, const Camera3D& camera, const f32v3& mousePickRay);
    void updateAndRenderUI(const vg::GBuffer* activeGBuffer, f32 elapsedSec);
    void renderEditorBrushDecals(const Camera3D& camera);

    bool hasActiveCenterPanel() const { return mActiveCenterPanel != nullptr; }
    IEditorViewportPanel* getActiveCenterPanel() const { return mActiveCenterPanel; }

private:
    void openModelForEdit(ModelDef& model);
    void openMaterialForEdit(MaterialHandle& materialHandle);
    void openFoliageForEdit(TileGrassData& grassData);
    void openBiomeForEdit();
    void openFishForEdit(FishDef& fishDef);
    void openParticleSystemForEdit(ParticleSystemDef* systemDef);
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
    std::unique_ptr<FishingEditorViewportPanel> mFishingEditorViewportPanel;
    std::unique_ptr<ParticleSystemEditorViewportPanel> mParticleSystemEditorViewportPanel;

    // Event listeners
    vui::KeyListeners mKeyListeners;
    vui::WindowListeners mWindowListeners;
};

