#pragma once

#include "IEditorViewportPanel.h"

#include <imgui.h>

// TODO: Move out?
class AssetEditorViewportPanelBase : public IEditorViewportPanel {
public:
    virtual const char* getViewportWindowName() const = 0;
};

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

    virtual void updateAndRenderInternal(f32 elapsedSec) = 0;
protected:
    AssetHandlePtr<T> mAssetHandle;
    T* mAssetData = nullptr;
    f32 mCurrentElapsedSec = 0.0f;
    f32v2 mViewportDims = f32v2(0.0f);
};

