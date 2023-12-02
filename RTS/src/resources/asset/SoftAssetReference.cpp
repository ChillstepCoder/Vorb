#include "stdafx.h"
#include "SoftAssetReference.h"

#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"
#include "ui/UIContext.h"
#include "resources/asset/AssetType.h"

#include "resources/ResourceManager.h"
#include "resources/IAssetRepository.h"

#include "rendering/material/MaterialData.h"
#include "definitions/AnimationDef.h"
#include "definitions/AnimMachineDef.h"
#include "definitions/BiomeDef.h"
#include "definitions/BrushDef.h"
#include "definitions/BuildingDef.h"
#include "definitions/BusinessDef.h"
#include "definitions/EffectDef.h"
#include "definitions/FishDef.h"
#include "definitions/RigDef.h"
#include "definitions/ModelDef.h"
#include "definitions/ParticleSystemDef.h"
#include "definitions/rendering/CubemapDef.h"
#include "definitions/rendering/TextureDef.h"
#include "definitions/SkillDef.h"
#include "definitions/TileGrassDef.h"
#include "item/ItemDef.h"
#include "tile/Tile.h"

static const f32v2 THUMBNAIL_SIZE = f32v2(50.0f);
static std::map<SoftAssetReference*, std::unique_ptr<ImguiUtil::AssetSelectorPopup>> sAssetSelectorPopup;

template <typename T>
void assetButton(SoftAssetReference& assetRef) {
    IAssetRepository<T>& repo = IAssetRepository<T>::getInstance();
    if (ImGui::Button(repo.getAssetTypeDisplayName())) {
        sAssetSelectorPopup[&assetRef] = std::make_unique<ImguiUtil::AssetSelectorPopup>(repo.getAssetRegistry());
        sAssetSelectorPopup[&assetRef]->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction<T>(), THUMBNAIL_SIZE);
    }
    ImGui::SameLine();
    ImGui::Text(assetRef.name.isValid() ? assetRef.name.toString().c_str() : "NONE");
    auto f = ImguiAssetThumbnails::getThumbnailFunction<T>();
    if (f && assetRef.name.isValid()) {
        f(repo.getAssetID(assetRef.name), f32v2(60.0f));
    }
}

bool ImguiUtil::updateAndRenderSoftAssetReference(const char* label, SoftAssetReference& assetRef) {

    ImGui::PushID(label);
    bool changed = false;

    ImGui::Separator();
    ImGui::Text(label);

    switch (assetRef.assetType) {
        case AssetType::Tile:
            assetButton<TileDef>(assetRef);
            break;
        case AssetType::ParticleSystem:
            assetButton<ParticleSystemDef>(assetRef);
            break;
        case AssetType::Effect:
            assetButton<EffectDef>(assetRef);
            break;
        case AssetType::Texture:
            assetButton<TextureDef>(assetRef);
            break;
        case AssetType::Cubemap:
            assetButton<CubemapDef>(assetRef);
            break;
        case AssetType::Brush:
            assetButton<BrushDef>(assetRef);
            break;
        case AssetType::Material:
            assetButton<MaterialDef>(assetRef);
            break;
        case AssetType::Rig:
            assetButton<RigDef>(assetRef);
            break;
        case AssetType::Animation:
            assetButton<AnimationDef>(assetRef);
            break;
        case AssetType::AnimMachine:
            assetButton<AnimMachineDef>(assetRef);
            break;
        case AssetType::Model:
            assetButton<ModelDef>(assetRef);
            break;
        case AssetType::Skill:
            assetButton<SkillDef>(assetRef);
            break;
        case AssetType::Item:
            assetButton<ItemDef>(assetRef);
            break;
        case AssetType::Fish:
            assetButton<FishDef>(assetRef);
            break;
        case AssetType::MaterialShader:
            assetButton<MaterialShaderDef>(assetRef);
            break;
        case AssetType::TileGrass:
            assetButton<TileGrassDef>(assetRef);
            break;
        case AssetType::Biome:
            assetButton<BiomeDef>(assetRef);
            break;
        case AssetType::NONE:
        default:
            panic("Unhandled asset type {} in SoftAssetReference::updateAndRenderSoftAssetReference", e_cast(assetRef.assetType));
            break;

    }
    static_assert(e_count(AssetType) == 17);

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
    ImGui::Separator();

    return changed;
}

AssetHandleBasePtr SoftAssetReference::getAssetHandle() const {
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
