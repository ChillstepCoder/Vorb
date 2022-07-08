#include "stdafx.h"
#include "TileInspectionPanel.h"

#include "tile/TileHandle.h"
#include "world/Chunk.h"
#include "resources/TileRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

TileInspectionPanel::TileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle) : mScreenPos(screenPos), mTileHandle(tileHandle) {

}

#define FLAG_DISPLAY(flag) ImGui::Text(" %-30s  %s", #flag, (tileHandle.tile->hasFlagMainThread(flag) != 0 ? "True" : "False")); ImGui::Separator();

inline void showTileFlagsMainThread(const TileHandle& tileHandle) {
    ImGui::Text("Flags:");
    FLAG_DISPLAY(TileFlags::TILE_FLAG_IS_INTERACTING);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_IS_STOCKPILE);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_IN_CITY);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_HAS_ITEM_STACK);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_DOOR);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    FLAG_DISPLAY(TileFlags::TILE_FLAG_IS_RESOURCE_RESERVED);

    static_assert(e_cast(TileFlags::TILE_FLAG_TERM) == 1 << 6, "Update");
}

inline void showTileLayerMainThread(const char* format, int layer, const TileHandle& tileHandle) {

    const ui32 id = tileHandle.tile->getLayersMainThread()[layer];
    if (id == TILE_ID_NONE) {
        ImGui::Text(format, id, "NONE");
    }
    else {
        ImGui::Text(format, id, TileRepository::getTileData(id).name.c_str());
    }
}

void TileInspectionPanel::updateAndRender() {

    const f32v2 panelDims(300.0f, 450.0f);
    const f32v2 clampedScreenPos = sMainGameWindowHandle->clampBoxPosToWindow(mScreenPos, panelDims);
    ImGui::SetNextWindowPos(ImVec2(clampedScreenPos.x, clampedScreenPos.y));
    ImGui::SetNextWindowSize(ImVec2(panelDims.x, panelDims.y));

    const TileContainer& container = *mTileHandle.container;
    const ui32v2 worldPos = container.getTileXYOffset(mTileHandle.tileIndex) + container.getWorldPos2D();

    ImGui::Begin("Inspect Tile", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("World Position: <%u, %u>", worldPos.x, worldPos.y);
    ImGui::Text("Base Z Position: %f", mTileHandle.tile->getGroundZPositionUncompressedMainThread());
    ImGui::Text("ChunkID: %u", ChunkID::fromWorldUI32v2(worldPos));
    ImGui::Separator();
    ImGui::Text("Layers:");
    showTileLayerMainThread("  Ground: %u %s", TILE_LAYER_GROUND, mTileHandle);
    showTileLayerMainThread("  Mid: %u %s", TILE_LAYER_MID, mTileHandle);
    showTileLayerMainThread("  Top: %u %s", TILE_LAYER_TOP, mTileHandle);
    ImGui::Separator();
    showTileFlagsMainThread(mTileHandle);

    ImGui::End();
}
