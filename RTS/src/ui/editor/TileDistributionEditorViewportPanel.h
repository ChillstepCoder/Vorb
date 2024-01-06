#pragma once
#include "AssetEditorViewportPanel.h"

#include "definitions/TileDistributionDef.h"
#include "rendering/MaterialShaderDef.h"

class TileDistributionEditorViewportPanel : public AssetEditorViewportPanel<TileDistributionDef> {
public:
    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Tile Distribution Editor"; }

private:
    void renderMesh() override;

    AssetHandlePtr<MaterialShaderDef> mImageShader;
};

