#include "stdafx.h"
#include "ImguiAssetThumbnails.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl2.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "Resources/TextureRepository.h"
#include "Resources/MaterialRepository.h"


// TODO: Move to ImguiThumbnais.h?
std::function<void(AssetID, f32v2)> ImguiAssetThumbnails::getMaterialThumbnailFunction() {
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
            ImGui::Text("UNLOADED");
        }
        ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255));
    };
}
