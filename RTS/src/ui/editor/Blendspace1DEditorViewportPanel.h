#pragma once

#include "AssetEditorViewportPanel.h"

#include "definitions/rendering/Blendspace1DDef.h"

class SkeletalAnimator;

struct Blendspace1DPlayerNode {
    f32 x = 0.0f;
    AssetID animId = INVALID_ASSET_ID;
};

struct AnimBlendPair {
    AssetID animId1 = INVALID_ASSET_ID;
    AssetID animId2 = INVALID_ASSET_ID;
    f32 weight1 = 1.0f; // Weight2 is 1 - weight1
};

class Blendspace1DPlayer {
public:
    Blendspace1DPlayer() = default;
    Blendspace1DPlayer(std::span<const Blendspace1DPlayerNode> inNodes) {
        nodes = std::make_unique<Blendspace1DPlayerNode[]>(inNodes.size());
        memcpy(nodes.get(), inNodes.data(), inNodes.size() * sizeof(Blendspace1DPlayerNode));
        numNodes = inNodes.size();
    }
    bool isValid() const { return nodes != nullptr; }

    AnimBlendPair getBlendPair(f32 x) const;

    std::unique_ptr<Blendspace1DPlayerNode[]> nodes;
    ui32 numNodes = 0;
    f32 mTime = 0.0f;
    f32 mSyncTime = 0.0f;
};

class Blendspace1DEditorViewportPanel : public AssetEditorViewportPanel<Blendspace1DDef>
{
public:
    Blendspace1DEditorViewportPanel();
    ~Blendspace1DEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
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
    f32 mLeaderAnimTime = 0.0f;
    f32 mSyncTime = 0.0f; // [0,1]
    bool mDraggingPreview = false;
    i32 mDragIndex = -1;
    i32 mSelectedIndex = -1;
    i32 mLeaderIndex = -1;

    Blendspace1DPlayer mBlendspacePlayer;
};

