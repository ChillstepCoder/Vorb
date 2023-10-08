#include "stdafx.h"
#include "TileInspectionPanel.h"

#include "tile/TileHandle.h"
#include "world/Chunk.h"
#include "world/IWorld.h"
#include "world/IChunkGrid.h"
#include "resources/TileRepository.h"

#include "debugging/DebugRenderer.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl2.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

TileInspectionPanel::TileInspectionPanel(IWorld& world, const f32v2& screenPos, const TileHandle& tileHandle) : mWorld(world), mScreenPos(screenPos), mTileHandle(tileHandle) {

}

#define FLAG_DISPLAY(flag) ImGui::TextColored(tile.hasFlag(flag) != 0 ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f)," %-30s  %s", #flag, (tile.hasFlag(flag) != 0 ? "True" : "False")); ImGui::Separator();

inline void showTileFlagsMainThread(const TileHandle& tileHandle) {
    ImGui::Text("Flags:");
    const Tile& tile = tileHandle.getTile();
    FLAG_DISPLAY(TileFlags::HAS_SOUTH_BLOCKER);
    FLAG_DISPLAY(TileFlags::HAS_WEST_BLOCKER);
    FLAG_DISPLAY(TileFlags::HAS_EAST_BLOCKER);
    FLAG_DISPLAY(TileFlags::HAS_NORTH_BLOCKER);
    FLAG_DISPLAY(TileFlags::HAS_DIAGONAL_BLOCKER);
    FLAG_DISPLAY(TileFlags::BLOCKED_BY_LARGE);
    FLAG_DISPLAY(TileFlags::MEDIUM_BLOCKER);
    FLAG_DISPLAY(TileFlags::LARGE_BLOCKER);
    FLAG_DISPLAY(TileFlags::IS_BLOCKED_BY_STRUCTURE);
    FLAG_DISPLAY(TileFlags::IS_INTERACTING);
    FLAG_DISPLAY(TileFlags::IS_STOCKPILE);
    FLAG_DISPLAY(TileFlags::HAS_ITEM_STACK);
    FLAG_DISPLAY(TileFlags::IS_RESOURCE_RESERVED);
    FLAG_DISPLAY(TileFlags::IN_CITY);

    static_assert(e_cast(TileFlags::TERM) == 1 << 13, "Update");
}

inline void showTileLayerMainThread(const char* format, int layer, const TileHandle& tileHandle) {

    const ui32 id = tileHandle.getTile().getLayers()[layer];
    if (id == TILE_ID_NONE) {
        ImGui::Text(format, id, "NONE");
    }
    else {
        ImGui::Text(format, id, TileRepository::get().getLoadedOrUnloadedAsset(id).name.c_str());
    }
}

void TileInspectionPanel::updateAndRender() {

    if (!mDidInit) {
        const f32v2 panelDims(420.0f, 600.0f);
        const f32v2 clampedScreenPos = sMainGameWindowHandle->clampBoxPosToWindow(mScreenPos, panelDims + f32v2(15.0f));
        ImGui::SetNextWindowPos(ImVec2(clampedScreenPos.x, clampedScreenPos.y));
        ImGui::SetNextWindowSize(ImVec2(panelDims.x, panelDims.y));
        mDidInit = true;
    }

    const TileContainer& container = *mTileHandle.container;
    const i32v3 xyzOffset = container.getTileSpatialGrid().getTileXYZOffset(mTileHandle.tileIndex);
    const i32v2 xyOffset(xyzOffset.x, xyzOffset.y);
    const i32v2 worldPos2D = i32v2(xyOffset) + container.getTileSpatialGrid().getWorldPos2D();
    const bool isOwned = container.isTileOwned(mTileHandle.tileIndex);

    // Debug tile cursor
    f32v3 worldPos3D = mTileHandle.getWorldPos3D();
    if (mTileHandle.getTile().getGroundZOffset()) {
        DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), color4(255, 128, 128, 180));
        worldPos3D.z += mTileHandle.getTile().getGroundZOffset();
    }
    if (isOwned) {
        DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), COLOR_WHITE);
    }
    else {
        DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), COLOR_RED);
    }
    {
        constexpr f32 AXIS_THICKNESS = 0.1f;
        DebugRenderer::drawLine(worldPos3D + f32v3(1.0f, 0.5f, 0.0f), f32v3(AXIS_THICKNESS, 0.0f, 0.0f), COLOR_RED);
        DebugRenderer::drawLine(worldPos3D + f32v3(0.5f, 1.0f, 0.0f), f32v3(0.0f, AXIS_THICKNESS, 0.0f), COLOR_GREEN);
    }

    ImGui::Begin("Inspect Tile", nullptr, ImGuiWindowFlags_NoCollapse);
    if (!isOwned) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "UNOWNED");
    }
    ImGui::Text("World Position: <%u, %u>", worldPos2D.x, worldPos2D.y);
    ImGui::Text("Base Z Position: %f", mTileHandle.getTile().getGroundZOffset());
    switch (container.getOwnerType()) {
        case TileContainerOwnerType::CHUNK:
            ImGui::Text("Owner Type: CHUNK");
            break;
        case TileContainerOwnerType::BUILDING:
            ImGui::Text("Owner Type: BUILDING");
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_cast(TileContainerOwnerType::COUNT) == 2);
    ImGui::Text("TileContainerID: %u", container.getId());
    ImGui::Text("ChunkID: %u", mWorld.getChunkGrid().getChunkIDFromWorldPos(worldPos2D));
    ImGui::Text("Tile Index: %u", mTileHandle.tileIndex);
    ImGui::Text("Container Offset: <%u,%u,%u>", xyzOffset.x, xyzOffset.y, xyzOffset.z);

    { // Select adjacent tiles
        if (ImGui::Button("x-1")) {
            if (xyzOffset.x > 0) {
                mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex - 1);
            }
        } ImGui::SameLine();
        if (ImGui::Button("x+1")) {
            if (xyzOffset.x < mTileHandle.container->getTileSpatialGrid().getDims().x - 1) {
                mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex + 1);
            }
        }
        if (ImGui::Button("y-1")) {
            if (xyzOffset.y > 0) {
                mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex - mTileHandle.container->getTileSpatialGrid().getDims().x);
            }
        } ImGui::SameLine();
        if (ImGui::Button("y+1")) {
            if (xyzOffset.y < mTileHandle.container->getTileSpatialGrid().getDims().y - 1) {
                mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex + mTileHandle.container->getTileSpatialGrid().getDims().x);
            }
        }
        if (mTileHandle.container->getTileSpatialGrid().getDims().z > 1) {
            if (ImGui::Button("z-1")) {
                if (xyzOffset.z > 0) {
                    mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex - mTileHandle.container->getTileSpatialGrid().getFloorStride());
                }
            } ImGui::SameLine();
            if (ImGui::Button("z+1")) {
                if (xyzOffset.z < mTileHandle.container->getTileSpatialGrid().getDims().z - 1) {
                    mTileHandle = TileHandle(mTileHandle.container, mTileHandle.tileIndex + mTileHandle.container->getTileSpatialGrid().getFloorStride());
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Layers:");
    showTileLayerMainThread("  Ground: %u %s", TILE_LAYER_GROUND, mTileHandle);
    showTileLayerMainThread("  Main: %u %s", TILE_LAYER_MAIN, mTileHandle);
    ImGui::Separator();
    showTileFlagsMainThread(mTileHandle);

    ImGui::End();
}
