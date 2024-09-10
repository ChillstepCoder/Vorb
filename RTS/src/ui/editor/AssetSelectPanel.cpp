#include "stdafx.h"
#include "AssetSelectPanel.h"

#include "resources/ResourceManager.h"

#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"
#include "ui/UIContext.h"
#include "ui/editor/EditorRoot.h"

#include <imgui.h>


static const f32v2 THUMBNAIL_SIZE = f32v2(50.0f);
static std::unique_ptr<ImguiUtil::AssetSelectorPopup> sVariantAssetSelectorPopup;
static std::vector<nString> sFilterNames;
static char sFilterBuf[64] = {};
static nString sFilterStr;

AssetSelectPanel::AssetSelectPanel() {
}

AssetSelectPanel::~AssetSelectPanel() {
}

AssetSelectPanelResult AssetSelectPanel::updateAndRender(float ySize) {

    AssetSelectPanelResult returnValue = std::make_pair(AssetSelectPanelResultCode::NONE, AssetDescriptor{});

    ImGui::BeginChild("Asset Selector", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Asset Selector");

    if (sVariantAssetSelectorPopup) {
        if (sVariantAssetSelectorPopup->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
            StrToken result = sVariantAssetSelectorPopup->getResult().mName;
            if (result.isValid()) {
                UIContext::getInstance().getEditorRoot().tryOpenAssetForEdit(sVariantAssetSelectorPopup->getResult().mDescriptor);
            }
            sVariantAssetSelectorPopup.reset();
        }
    }
    else {
        if (ImGui::InputText("Filter", sFilterBuf, 64)) {
            sFilterStr = sFilterBuf;
            std::transform(sFilterStr.begin(), sFilterStr.end(), sFilterStr.begin(), [](unsigned char c) { return std::tolower(c); });
        }


        for (int i = 0; i < e_count(AssetType); ++i) {
            const ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 0);
            const char* name = ENUM_CSTR(AssetType, (AssetType)i);
            nString nameLower = name; 
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (sFilterStr.empty() || nameLower.find(sFilterStr) != nameLower.npos) {
                if (ImGui::Button(name, size)) {
                    auto& repo = ResourceManager::get().getAssetRepository((AssetType)i);
                    sVariantAssetSelectorPopup = std::make_unique<ImguiUtil::AssetSelectorPopup>(repo.getAssetRegistry());
                    sVariantAssetSelectorPopup->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction((AssetType)i), THUMBNAIL_SIZE);
                    break;
                }
            }
        }
    }

    ImGui::EndChild();

    return returnValue;
}
