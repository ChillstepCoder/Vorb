#pragma once

#include "IEditorViewportPanel.h"

class MaterialShaderDef;
class ModelDef;
class AnimationDef;

struct AnimSampleBlendData {
    const AnimationDef* anim;
    f32 weight;
    f32 animTime;
};

class AssetEditorViewportPanelBase : public IEditorViewportPanel {
public:
    AssetEditorViewportPanelBase();
    virtual ~AssetEditorViewportPanelBase();

    virtual const char* getViewportWindowName() const = 0;
    virtual void setCurrentAsset(AssetID assetId) = 0;

protected:
    const MaterialShaderDef* getModelRenderShader() const;
    void renderMeshStatic(const ModelDef* modelAsset, int variantIndex, int lod, bool showSingleSubmesh, int singleSubmeshIndex);
    void renderMeshSkeletal(const ModelDef* modelAsset, int variantIndex, int lod, const AnimationDef* previewAnim, f32 previewAnimTime);
    void renderMeshSkeletalBlended(const ModelDef* modelAsset, int variantIndex, int lod, const std::span<AnimSampleBlendData> anims);

    mutable AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
};