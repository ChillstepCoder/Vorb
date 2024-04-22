#pragma once

#include "AssetEditorViewportPanel.h"

#include "definitions/AnimMachineDef.h"

class AnimMachineEditorViewportPanel : public AssetEditorViewportPanel<AnimMachineDef> {
public:
    AnimMachineEditorViewportPanel();
    ~AnimMachineEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
    bool hasBottomControls() const override { return true; }
    void updateAndRenderBottomControls() override;

    const char* getViewportWindowName() const override { return "AnimMachine Editor"; }

private:
    const MaterialShaderDef* getShader() override;
    void renderMesh() override;
    void onChanged();
};

