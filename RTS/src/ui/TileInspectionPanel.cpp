#include "stdafx.h"
#include "TileInspectionPanel.h"

#include "world/TileHandle.h"
#include "world/Chunk.h"
#include "world/TileRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

TileInspectionPanel::TileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle) : mScreenPos(screenPos), mTileHandle(tileHandle) {

}

#define FLAG_DISPLAY(flag) ImGui::Text(" %-30s  %s", #flag, (tileHandle.tile->hasFlagMainThread(flag) != 0 ? "True" : "False")); ImGui::Separator();

inline void showTileFlagsMainThread(const TileHandle& tileHandle) {
    ImGui::Text("Flags:");
    FLAG_DISPLAY(TILE_FLAG_IS_INTERACTING);
    FLAG_DISPLAY(TILE_FLAG_IS_STOCKPILE);
    FLAG_DISPLAY(TILE_FLAG_IN_CITY);
    FLAG_DISPLAY(TILE_FLAG_HAS_ITEM_STACK);
    FLAG_DISPLAY(TILE_FLAG_IS_BUILDING);
    FLAG_DISPLAY(TILE_FLAG_IS_LARGE_OBJECT_ROOT);
    FLAG_DISPLAY(TILE_FLAG_DOOR);
    FLAG_DISPLAY(TILE_FLAG_BREAKABLE);
    FLAG_DISPLAY(TILE_FLAG_ROAD);
    FLAG_DISPLAY(TILE_FLAG_HAS_ROOF);
    FLAG_DISPLAY(TILE_FLAG_HAS_COLLIDER);
    FLAG_DISPLAY(TILE_FLAG_QUEUED_UPDATE);
    FLAG_DISPLAY(TILE_FLAG_IS_RESOURCE_RESERVED);

    static_assert(TILE_FLAG_TERM == 1 << 13, "Update");
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

    const Chunk& chunk = *mTileHandle.chunk;
    const ui32v2 worldPos = ui32v2(chunk.getWorldPos().x + mTileHandle.index.getX(), chunk.getWorldPos().y + mTileHandle.index.getY());

    ImGui::Begin("Inspect Tile", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("World Position: <%u, %u>", worldPos.x, worldPos.y);
    ImGui::Text("Base Z Position: %f", mTileHandle.tile->getBaseZPositionUncompressedMainThread());
    ImGui::Text("ChunkID: %u", chunk.getChunkID().id);
    ImGui::Separator();
    ImGui::Text("Layers:");
    showTileLayerMainThread("  Ground: %u %s", TILE_LAYER_GROUND, mTileHandle);
    showTileLayerMainThread("  Mid: %u %s", TILE_LAYER_MID, mTileHandle);
    showTileLayerMainThread("  Top: %u %s", TILE_LAYER_TOP, mTileHandle);
    ImGui::Separator();
    showTileFlagsMainThread(mTileHandle);

    ImGui::End();
}
