#pragma once

#include "IEditorViewportPanel.h"

class MaterialShaderDef;

class AssetEditorViewportPanelBase : public IEditorViewportPanel {
public:
    AssetEditorViewportPanelBase();
    virtual ~AssetEditorViewportPanelBase();

    virtual const char* getViewportWindowName() const = 0;
    virtual void setCurrentAsset(AssetID assetId) = 0;

protected:
    const MaterialShaderDef* getModelRenderShader() const;

    mutable AssetHandlePtr<MaterialShaderDef> mPbrMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mEditorMaterial;
    mutable AssetHandlePtr<MaterialShaderDef> mWireframeMaterial;
};