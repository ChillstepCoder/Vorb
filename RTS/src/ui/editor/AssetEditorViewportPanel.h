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
        ImGui::Begin(getViewportWindowName(), &isOpen, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        mViewportDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
        if (mViewportDims.x < 1 || mViewportDims.y < 1) {
            ImGui::End();
            return isOpen;
        }

        updateCamera(mViewportDims.x / mViewportDims.y);

        updateFramebufferAndLazyInit(mViewportDims);

        clearFramebuffers();

        updateAndRenderInternal(elapsedSec);

        renderCenterPanel(nullptr);

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

    virtual void updateAndRenderInternal(f32 elapsedSec) { UNUSED(elapsedSec); }
      
    virtual void updateAndRenderSaveButton() {
        if (ImGui::Button("Save")) {
            IAssetRepository<T>& repo = IAssetRepository<T>::getInstance();
            if (!repo.saveAsset(mAssetData->getID())) {
                panic("Failed to save asset {}", repo.getAssetFilePath(mAssetData->getID()).getCString());
            }
        }
    }
protected:
    AssetHandlePtr<T> mAssetHandle;
    T* mAssetData = nullptr;
    f32 mCurrentElapsedSec = 0.0f;
    f32v2 mViewportDims = f32v2(0.0f);
    bool mAssetWasChanged = true; // Always starts true for initialization
};

