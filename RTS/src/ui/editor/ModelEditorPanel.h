#pragma once

#include "IEditorViewportPanel.h"

struct ModelDef;

class ModelEditorPanel : public IEditorViewportPanel
{
public:
    ModelEditorPanel();
    ~ModelEditorPanel();

    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void setModel(ModelDef& model) { mCurrentModel = &model; }

private:
    VGTexture renderModelToTexture();
    VGTexture renderModelToTextureBlendTest();
    VGTexture renderModelToTextureEdgeTest();
    VGTexture renderModelToTexturePBRTest();

    ModelDef* mCurrentModel = nullptr;
   
    bool mDirtyModelData = false;
    int mLod = 0;

    // Blend test
    int mBlendTestPasses = 1;
    f32 mBlendTestRadius = 7.0f;
    f32 mBlendTestNormThreshold = 0.016f;
    f32 mBlendTestDepthThreshold = 0.104f;
    int mBlendTestDisplayMode = 0;
    bool mBlendTestShowVariance = 0;
    bool mBlendTestShowEdges = 0;
    bool mBlendTestDisable = 0;

    // Edge test //TODO: She likes this blurrier
    float mEdgeTestThreshold = 0.08f;
    f32 mEdgeTestDepthThreshold = 0.05f;
    int mEdgeTestDisplayMode = 0;
    int mEdgeSize = 10;
    int mEdgeBlendPasses = 3;
    f32 mEdgeBlendRadius = 1.315f;
    bool mEdgeTestDisable = 0;
    bool mEdgeTestShowEdges = 0;
};

