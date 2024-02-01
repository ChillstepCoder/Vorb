#pragma once
#include "AssetEditorViewportPanel.h"

#include "definitions/TileDef.h"

class TileEditorViewportPanel : public AssetEditorViewportPanel<TileDef>
{
public:
    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Tile Editor"; }

private:
    const MaterialShaderDef* getShader() override;
    void renderMesh() override;
    int mVariantIndex = 0;
};