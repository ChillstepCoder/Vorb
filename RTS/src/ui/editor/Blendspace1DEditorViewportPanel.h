#pragma once

#include "AssetEditorViewportPanel.h"

#include "definitions/AnimationDef.h"
#include "definitions/rendering/Blendspace1DDef.h"
#include "rendering/animation/Blendspace1DPlayer.h"

class SkeletalAnimator;

class Blendspace1DEditorViewportPanel : public AssetEditorViewportPanel<Blendspace1DDef>
{
public:
    Blendspace1DEditorViewportPanel();
    ~Blendspace1DEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
    bool updateAndRenderTertiaryControls(f32 ySize) override;
    bool hasBottomControls() const override { return true; }
    void updateAndRenderBottomControls() override;

    const char* getViewportWindowName() const override { return "Blendspace 1D Editor"; }

private:
    const MaterialShaderDef* getShader() override;
    void renderMesh() override;
    void onChanged();
    void rebuildBlendspacePlayer();

    // Skeletal
    std::unique_ptr<SkeletalAnimator> mSkeletalAnimator;
    SoftAssetReference mPreviewModel = SoftAssetReference(AssetType::Model);
    f32 mPreviewX = 0.0f;
    f32 mPreviewAnimTime = 0.0f;
    f32 mSyncAlpha = 0.0f; // [0,1]
    bool mDraggingPreview = false;
    i32 mDragIndex = -1;
    i32 mSelectedIndex = -1;
    i32 mHoverIndex = -1;

    Blendspace1DPlayer mBlendspacePlayer;
};

