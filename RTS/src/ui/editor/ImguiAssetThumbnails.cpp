#include "stdafx.h"
#include "ImguiAssetThumbnails.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "definitions/AssetDefinitions.h"

#include "Resources/TextureRepository.h"
#include "Resources/MaterialRepository.h"


std::function<void(AssetID, f32v2)> ImguiAssetThumbnails::getThumbnailFunction(AssetType assetType) {
    switch (assetType) {
        case AssetType::Tile:
            return getThumbnailFunction<TileDef>();
        case AssetType::ParticleSystem:
            return getThumbnailFunction<ParticleSystemDef>();
        case AssetType::Effect:
            return getThumbnailFunction<EffectDef>();
        case AssetType::Texture:
            return getThumbnailFunction<TextureDef>();
        case AssetType::Cubemap:
            return getThumbnailFunction<CubemapDef>();
        case AssetType::Brush:
            return getThumbnailFunction<BrushDef>();
        case AssetType::Material:
            return getThumbnailFunction<MaterialDef>();
        case AssetType::Rig:
            return getThumbnailFunction<RigDef>();
        case AssetType::Animation:
            return getThumbnailFunction<AnimationDef>();
        case AssetType::AnimMachine:
            return getThumbnailFunction<AnimMachineDef>();
        case AssetType::Model:
            return getThumbnailFunction<ModelDef>();
        case AssetType::Skill:
            return getThumbnailFunction<SkillDef>();
        case AssetType::Item:
            return getThumbnailFunction<ItemDef>();
        case AssetType::Fish:
            return getThumbnailFunction<FishDef>();
        case AssetType::MaterialShader:
            return getThumbnailFunction<MaterialShaderDef>();
        case AssetType::TileGrass:
            return getThumbnailFunction<TileGrassDef>();
        case AssetType::Biome:
            return getThumbnailFunction<BiomeDef>();
        case AssetType::TileDistribution:
            return getThumbnailFunction<TileDistributionDef>();
        case AssetType::NONE:
        default:
            panic("Unhandled asset type {} in ImguiAssetThumbnails::getThumbnailFunction", e_cast(assetType));
            break;

    }
    static_assert(e_count(AssetType) == 18);
}

template<>
std::function<void(AssetID, f32v2)> ImguiAssetThumbnails::getThumbnailFunction<MaterialDef>() {
    return [](AssetID id, f32v2 dims) {
        ImVec2 pMin = ImGui::GetCursorScreenPos();
        ImVec2 pMax = ImVec2(pMin.x + dims.x, pMin.y + dims.y);
        ImVec2 idims(dims.x, dims.y);
        const bool visibleImage = ImGui::IsRectVisible(idims);
        bool shouldRenderDummy = true;
        if (visibleImage) {
            if (id != INVALID_MATERIAL_ID) {
                const MaterialDef& def = MaterialRepository::get().getLoadedOrUnloadedAsset(id);
                StrToken name = def.albedoTexture;
                if (!name.isValid()) {
                    name = def.getName();
                }
                const TextureDef* tdef = TextureRepository::get().tryGetLoadedAsset(name);
                // Get the current cursor position(top - left corner of the image)

                if (tdef) {
                    ImGui::Image((ImTextureID)tdef->gpuTexture.getHandle(), idims);
                    shouldRenderDummy = false;
                }
            }
        }
        if (shouldRenderDummy) {
            ImGui::Image((ImTextureID)0, idims);
            ImGui::SameLine();
            if (ImGui::Button("UNLOADED")) {
                // Force a load to begin
                MaterialRepository::get().getAssetHandle(id);
            }
        }
        ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
    };
}

template<>
std::function<void(AssetID, f32v2)> ImguiAssetThumbnails::getThumbnailFunction<TextureDef>() {
    return [](AssetID id, f32v2 dims) {
        ImVec2 pMin = ImGui::GetCursorScreenPos();
        ImVec2 pMax = ImVec2(pMin.x + dims.x, pMin.y + dims.y);
        ImVec2 idims(dims.x, dims.y);
        const bool visibleImage = ImGui::IsRectVisible(idims);
        bool shouldRenderDummy = true;
        if (visibleImage) {
            if (id != INVALID_MATERIAL_ID) {
                const TextureDef* tdef = TextureRepository::get().tryGetLoadedAsset(id);
                // Get the current cursor position(top - left corner of the image)

                if (tdef) {
                    ImGui::Image((ImTextureID)tdef->gpuTexture.getHandle(), idims);
                    shouldRenderDummy = false;
                }
            }
        }
        if (shouldRenderDummy) {
            ImGui::Image((ImTextureID)0, idims);
            ImGui::SameLine();
            if (ImGui::Button("UNLOADED")) {
                // Force a load to begin
                TextureRepository::get().getAssetHandle(id);
            }
        }
        ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
    };
}
