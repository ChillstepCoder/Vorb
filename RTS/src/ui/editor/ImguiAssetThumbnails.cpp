#include "stdafx.h"
#include "ImguiAssetThumbnails.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "Resources/TextureRepository.h"
#include "Resources/MaterialRepository.h"

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
