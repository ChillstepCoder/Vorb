#pragma once
#include "AssetEditorViewportPanel.h"

#include "definitions/TileDistributionDef.h"
#include "rendering/MaterialShaderDef.h"
#include "generation/TileDistributionPreviewTexture.h"

class TileDistributionEditorViewportPanel : public AssetEditorViewportPanel<TileDistributionDef> {
public:
    TileDistributionEditorViewportPanel();
    ~TileDistributionEditorViewportPanel();
    void updateAndRenderPrimaryControls(f32 ySize) override;

    const char* getViewportWindowName() const override { return "Tile Distribution Editor"; }
    void postCenterPanelRender(const i32AABB2&) override;

private:
    void renderMesh() override;

    TileDistributionPreviewTexture mPreviewTexture;
    bool mShowThreshold = false;
    bool mShowSpawns = true;
    bool mTryPrecalc = true;
    bool mDirtyView = true;
    bool mSkipPrecalc = false;
    float mDensityMult = 1.0f;
    i32v2 mTileOffset = {};
    f64 mLastGenerationTimeMs = 0.0;
    i32 mLastTextureSize = 128;
    i32 mTextureSize = 128;
};

