#pragma once
#include "AssetEditorViewportPanel.h"

#include "item/ItemDef.h"

class ItemEditorViewportPanel : public AssetEditorViewportPanel<ItemDef>
{
public:
    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Item Editor"; }

private:
    const MaterialShaderDef* getShader() override;
    void renderMesh() override;
};

