#pragma once

#include "IEditorViewportPanel.h"

class AssetEditorViewportPanelBase : public IEditorViewportPanel {
public:
    virtual const char* getViewportWindowName() const = 0;
    virtual void setCurrentAsset(AssetID assetId) = 0;
};