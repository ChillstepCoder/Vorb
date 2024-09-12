#include "stdafx.h"
#include "VariantAssetRef.h"

#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"
#include "ui/UIContext.h"
#include "ui/editor/EditorRoot.h"
#include "resources/asset/AssetType.h"

#include "resources/ResourceManager.h"
#include "resources/IAssetRepository.h"

#include "resources/asset/LiteAssetRef.h"

static const f32v2 THUMBNAIL_SIZE = f32v2(50.0f);
static std::map<ui64, std::unique_ptr<ImguiUtil::AssetSelectorPopup>> sVariantAssetSelectorPopup;
static std::map<ui64, std::unique_ptr<ImguiUtil::AssetSelectorPopup>> sAssetSelectorPopup;


bool assetButton(VariantAssetRef& assetRef, ui64 id, AssetType type, AssetFilterFunc fiterFunc) {
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(type);
    ImguiUtil::ScopedColor color(ImGuiCol_Button, assetRef.isValid() ? ImguiColors::Theme::highlight : ImguiColors::Theme::error);
    if (ImGui::ButtonEx(repo.getAssetTypeDisplayName(), ImVec2(0,0), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle)) {
        // If it was a middle click, navigate to the asset if it is valid
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
            if (assetRef.isValid()) {
                UIContext::getInstance().getEditorRoot().tryOpenAssetForEdit(assetRef.getAssetDescriptor());
            }
        }
        else {
            sVariantAssetSelectorPopup[id] = std::make_unique<ImguiUtil::AssetSelectorPopup>(repo.getAssetRegistry());
            sVariantAssetSelectorPopup[id]->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction(type), THUMBNAIL_SIZE);
            sVariantAssetSelectorPopup[id]->setAssetFilterFunc(fiterFunc);
        }
    }
    ImGui::SameLine();
    if (assetRef.isValid()) {
        if (ImGui::Button("x")) {
            assetRef.name = StrToken();
            return true;
        }
    }
    ImGui::SameLine();
    ImGui::Text(assetRef.name.isValid() ? assetRef.name.toString().c_str() : "NONE");
    auto f = ImguiAssetThumbnails::getThumbnailFunction(type);
    if (f && assetRef.name.isValid()) {
        f(repo.getAssetID(assetRef.name), f32v2(60.0f));
    }
    return false;
}

bool assetButton(LiteAssetRefBase& assetRef, ui64 id, AssetType type, AssetFilterFunc fiterFunc) {
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(type);
    ImguiUtil::ScopedColor color(ImGuiCol_Button, assetRef.isValid() ? ImguiColors::Theme::highlight : ImguiColors::Theme::error);
    if (ImGui::ButtonEx(repo.getAssetTypeDisplayName(), ImVec2(0, 0), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle)) {
        // If it was a middle click, navigate to the asset if it is valid
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
            if (assetRef.isValid()) {
                UIContext::getInstance().getEditorRoot().tryOpenAssetForEdit(AssetDescriptor{.id = assetRef.getAssetID(), .assetType = type});
            }
        }
        else {
            sAssetSelectorPopup[id] = std::make_unique<ImguiUtil::AssetSelectorPopup>(repo.getAssetRegistry());
            sAssetSelectorPopup[id]->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction(type), THUMBNAIL_SIZE);
            sAssetSelectorPopup[id]->setAssetFilterFunc(fiterFunc);
        }
    }
    ImGui::SameLine();
    if (assetRef.isValid()) {
        if (ImGui::Button("x")) {
            assetRef.invalidate();
            return true;
        }
    }
    StrToken name;
    if (assetRef.isValid()) {
        name = repo.getAssetName(assetRef.getAssetID());
    }

    ImGui::SameLine();
    ImGui::Text(name.isValid() ? name.toString().c_str() : "NONE");
    auto f = ImguiAssetThumbnails::getThumbnailFunction(type);
    if (f && name.isValid()) {
        f(assetRef.getAssetID(), f32v2(60.0f));
    }
    return false;
}

template <typename T>
bool assetButtons(T& assetRef, ui64 buttonId, AssetType type, const char* label, AssetFilterFunc filterFunc) {
    ImGui::PushID(&assetRef);
    bool changed = false;

    if (label) {
        ImGui::Text(label);
        ImGui::SameLine();
    }

    switch (type) {
        case AssetType::Tile:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::ParticleSystem:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Effect:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Texture:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Cubemap:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Brush:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Material:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Rig:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Animation:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::AnimMachine:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Blendspace1D:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Model:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Skill:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Item:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Fish:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::MaterialShader:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::TileGrass:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Biome:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::TileDistribution:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Building:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::Room:
            changed |= assetButton(assetRef, buttonId, type, filterFunc);
            break;
        case AssetType::NONE:
            ImGui::Button("INVALID SOFT REF");
            break;
        default:
            panic("Unhandled asset type {} in SoftAssetReference::updateAndRenderSoftAssetReference", e_cast(type));
            break;

    }
    static_assert(e_count(AssetType) == 22);

    return changed;
}

bool ImguiUtil::updateAndRenderVariantAssetReference(
    const char* label, VariantAssetRef& assetRef, ui64 buttonUid, std::function<bool(AssetID)> filterFunc /*= nullptr*/
) {

    bool changed = assetButtons(assetRef, buttonUid, assetRef.assetType, label, filterFunc);

    auto&& it = sVariantAssetSelectorPopup.find(buttonUid);
    if (it != sVariantAssetSelectorPopup.end()) {

        if (it->second->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
            StrToken result = it->second->getResult().mName;
            if (result.isValid()) {
                assetRef.name = it->second->getResult().mName;
            }
            it->second.reset();
            changed = true;
            sVariantAssetSelectorPopup.erase(it);
        }
    }

    ImGui::PopID();

    return changed;
}

bool ImguiUtil::updateAndRenderVariantAssetReference(const char* label, VariantAssetRef& assetRef, AssetFilterFunc filterFunc /*= nullptr*/) {
    return updateAndRenderVariantAssetReference(label, assetRef, (ui64)&assetRef, filterFunc);
}

bool ImguiUtil::updateAndRenderAssetReference(const char* label, LiteAssetRefBase& assetRef, ui64 buttonUid, AssetType type, AssetFilterFunc filterFunc /*= nullptr*/) {

    bool changed = assetButtons(assetRef, buttonUid, type, label, filterFunc);

    auto&& it = sAssetSelectorPopup.find(buttonUid);
    if (it != sAssetSelectorPopup.end()) {

        if (it->second->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
            StrToken result = it->second->getResult().mName;
            if (result.isValid()) {
                assetRef.setAssetID(it->second->getResult().getId());
            }
            it->second.reset();
            changed = true;
            sAssetSelectorPopup.erase(it);
        }
    }

    ImGui::PopID();

    return changed;
}

bool ImguiUtil::updateAndRenderAssetReference(const char* label, LiteAssetRefBase& assetRef, AssetType type, AssetFilterFunc filterFunc /*= nullptr*/) {
    return updateAndRenderAssetReference(label, assetRef, (ui64)&assetRef, type, filterFunc);
}

AssetHandleBasePtr VariantAssetRef::getAssetHandleBase() const {
    if (!isValid()) {
        return nullptr;
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(assetType);
    return repo.getAssetHandleBase(name);
}

AssetID VariantAssetRef::getAssetID() const {
    if (!isValid()) {
        return INVALID_ASSET_ID;
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(assetType);
    return repo.getAssetID(name);
}
