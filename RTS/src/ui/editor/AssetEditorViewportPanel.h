#pragma once

#include <imgui.h>

#include "AssetEditorViewportPanelBase.h"
#include "resources/IAssetRepository.h"

template <typename T>
class AssetEditorViewportPanel : public AssetEditorViewportPanelBase {
public:
    bool updateAndRender(f32 elapsedSec) override {
        mCurrentElapsedSec = elapsedSec;

        if (mAssetHandle) {
            // Editor can mutate
            mAssetData = const_cast<T*>(mAssetHandle->tryGetLoadedAsset());
        }
        else {
            mAssetData = nullptr;
        }

        bool isOpen = true;
        ImGui::Begin(getViewportWindowName(), &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        mViewportDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
        updateCamera(mViewportDims.x / mViewportDims.y);

        updateFramebufferAndLazyInit(mViewportDims);

        clearFramebuffers();

        updateAndRenderInternal(elapsedSec);

        ImGui::End();
        return isOpen;
    }

    void setCurrentAsset(AssetID assetId) override {
        if (assetId == INVALID_ASSET_ID) {
            mAssetHandle = nullptr;
            mAssetData = nullptr;
        }
        else {
            mAssetHandle = IAssetRepository<T>::getInstance().getAssetHandle(assetId);
        }
        mAssetWasChanged = true;
    }

    virtual void updateAndRenderInternal(f32 elapsedSec) = 0;
protected:
    AssetHandlePtr<T> mAssetHandle;
    T* mAssetData = nullptr;
    f32 mCurrentElapsedSec = 0.0f;
    f32v2 mViewportDims = f32v2(0.0f);
    bool mAssetWasChanged = true; // Always starts true for initialization
};

