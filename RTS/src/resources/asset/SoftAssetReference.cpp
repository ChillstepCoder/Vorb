#include "stdafx.h"
#include "SoftAssetReference.h"

#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"
#include "ui/UIContext.h"
#include "ui/editor/EditorRoot.h"
#include "resources/asset/AssetType.h"

#include "resources/ResourceManager.h"
#include "resources/IAssetRepository.h"

#include "rendering/material/MaterialData.h"
#include "definitions/AssetDefinitions.h"

static const f32v2 THUMBNAIL_SIZE = f32v2(50.0f);
static std::map<SoftAssetReference*, std::unique_ptr<ImguiUtil::AssetSelectorPopup>> sAssetSelectorPopup;

template <typename T>
bool assetButton(SoftAssetReference& assetRef) {
    IAssetRepository<T>& repo = IAssetRepository<T>::getInstance();
    ImguiUtil::ScopedColor color(ImGuiCol_Button, assetRef.isValid() ? ImguiColors::Theme::highlight : ImguiColors::Theme::error);
    if (ImGui::ButtonEx(repo.getAssetTypeDisplayName(), ImVec2(0,0), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle)) {
        // If it was a middle click, navigate to the asset if it is valid
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
            if (assetRef.isValid()) {
                UIContext::getInstance().getEditorRoot().tryOpenAssetForEdit(assetRef.getAssetDescriptor());
            }
        }
        else {
            sAssetSelectorPopup[&assetRef] = std::make_unique<ImguiUtil::AssetSelectorPopup>(repo.getAssetRegistry());
            sAssetSelectorPopup[&assetRef]->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction<T>(), THUMBNAIL_SIZE);
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
    auto f = ImguiAssetThumbnails::getThumbnailFunction<T>();
    if (f && assetRef.name.isValid()) {
        f(repo.getAssetID(assetRef.name), f32v2(60.0f));
    }
    return false;
}

bool ImguiUtil::updateAndRenderSoftAssetReference(const char* label, SoftAssetReference& assetRef) {

    ImGui::PushID(&assetRef);
    bool changed = false;

    if (label) {
        ImGui::Text(label);
        ImGui::SameLine();
    }

    switch (assetRef.assetType) {
        case AssetType::Tile:
            changed |= assetButton<TileDef>(assetRef);
            break;
        case AssetType::ParticleSystem:
            changed |= assetButton<ParticleSystemDef>(assetRef);
            break;
        case AssetType::Effect:
            changed |= assetButton<EffectDef>(assetRef);
            break;
        case AssetType::Texture:
            changed |= assetButton<TextureDef>(assetRef);
            break;
        case AssetType::Cubemap:
            changed |= assetButton<CubemapDef>(assetRef);
            break;
        case AssetType::Brush:
            changed |= assetButton<BrushDef>(assetRef);
            break;
        case AssetType::Material:
            changed |= assetButton<MaterialDef>(assetRef);
            break;
        case AssetType::Rig:
            changed |= assetButton<RigDef>(assetRef);
            break;
        case AssetType::Animation:
            changed |= assetButton<AnimationDef>(assetRef);
            break;
        case AssetType::AnimMachine:
            changed |= assetButton<AnimMachineDef>(assetRef);
            break;
        case AssetType::Blendspace1D:
            changed |= assetButton<Blendspace1DDef>(assetRef);
            break;
        case AssetType::Model:
            changed |= assetButton<ModelDef>(assetRef);
            break;
        case AssetType::Skill:
            changed |= assetButton<SkillDef>(assetRef);
            break;
        case AssetType::Item:
            changed |= assetButton<ItemDef>(assetRef);
            break;
        case AssetType::Fish:
            changed |= assetButton<FishDef>(assetRef);
            break;
        case AssetType::MaterialShader:
            changed |= assetButton<MaterialShaderDef>(assetRef);
            break;
        case AssetType::TileGrass:
            changed |= assetButton<TileGrassDef>(assetRef);
            break;
        case AssetType::Biome:
            changed |= assetButton<BiomeDef>(assetRef);
            break;
        case AssetType::TileDistribution:
            changed |= assetButton<TileDistributionDef>(assetRef);
            break;
        case AssetType::Building:
            changed |= assetButton<BuildingDef>(assetRef);
            break;
        case AssetType::Room:
            changed |= assetButton<RoomDef>(assetRef);
            break;
        case AssetType::NONE:
        default:
            panic("Unhandled asset type {} in SoftAssetReference::updateAndRenderSoftAssetReference", e_cast(assetRef.assetType));
            break;

    }
    static_assert(e_count(AssetType) == 21);

    auto&& it = sAssetSelectorPopup.find(&assetRef);
    if (it != sAssetSelectorPopup.end()) {
        if (it->second->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
            StrToken result = it->second->getResult().mName;
            if (result.isValid()) {
                assetRef.name = it->second->getResult().mName;
            }
            it->second.reset();
            changed = true;
            sAssetSelectorPopup.erase(it);
        }
    }

    ImGui::PopID();

    return changed;
}

AssetHandleBasePtr SoftAssetReference::getAssetHandleBase() const {
    if (!isValid()) {
        return nullptr;
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(assetType);
    return repo.getAssetHandleBase(name);
}

AssetID SoftAssetReference::getAssetID() const {
    if (!isValid()) {
        return INVALID_ASSET_ID;
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(assetType);
    return repo.getAssetID(name);
}
