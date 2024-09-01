#pragma once

#include "IEditorViewportPanel.h"
#include "definitions/AnimationDef.h"

class MaterialShaderDef;
class ModelDef;
class AnimationDef;

class AssetEditorViewportPanelBase : public IEditorViewportPanel {
public:
    AssetEditorViewportPanelBase();
    virtual ~AssetEditorViewportPanelBase();

    virtual const char* getViewportWindowName() const = 0;
    virtual void setCurrentAsset(AssetID assetId) = 0;

protected:
    const MaterialShaderDef* getModelRenderShader() const;
    void renderMeshStatic(
        const ModelDef* modelAsset, int variantIndex, int lod, bool showSingleSubmesh, int singleSubmeshIndex, f32 crossfade = -MATH_EPSILON
    );
    void renderMeshSkeletal(
        const ModelDef* modelAsset, int variantIndex, int lod, const AnimationDef* previewAnim, f32 previewAnimTime, f32 crossfade = -MATH_EPSILON
    );
    void renderMeshSkeletalBlended(
        const ModelDef* modelAsset, int variantIndex, int lod, const std::span<AnimSampleBlendData> anims, f32 crossfade = -MATH_EPSILON
    );

    mutable AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
};