#pragma once
#include "AssetEditorViewportPanel.h"

#include "definitions/TileDistributionDef.h"
#include "rendering/MaterialShaderDef.h"

class TileDistributionEditorViewportPanel : public AssetEditorViewportPanel<TileDistributionDef> {
public:
    TileDistributionEditorViewportPanel();
    ~TileDistributionEditorViewportPanel();
    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Tile Distribution Editor"; }

private:
    void renderMesh() override;

    AssetHandlePtr<MaterialShaderDef> mImageShader;
    VGTexture mThresholdTexture = 0;
    VGTexture mSpawnTexture = 0;
    bool mShowThreshold = true;
    bool mShowSpawns = true;
    bool mDirtyView = true;
    float mDensityMult = 0.5f;
    i32v2 mTileOffset = {};
};

