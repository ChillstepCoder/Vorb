#include "stdafx.h"
#include "TileInspectionPanel.h"

#include "tile/TileHandle.h"
#include "world/Chunk.h"
#include "world/World.h"
#include "world/IChunkGrid.h"
#include "resources/TileRepository.h"

#include "debugging/DebugRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "building/building.h"


#include "ui/GameWindow.h"

TileInspectionPanel::TileInspectionPanel(World& world, const f32v2& screenPos, const TileHandle& tileHandle) : mWorld(world), mScreenPos(screenPos), mTileHandle(tileHandle) {

}

#define FLAG_DISPLAY(flag) ImGui::TextColored(tile.hasFlag(flag) != 0 ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f)," %-30s  %s", #flag, (tile.hasFlag(flag) != 0 ? "True" : "False")); ImGui::Separator();

inline void showTileFlagsMainThread(const TileHandle& tileHandle) {
    ImGui::Text("Flags:");
    const Tile& tile = tileHandle.getTile();
    FLAG_DISPLAY(TileFlags::IS_BLOCKED_BY_BUILDING);
    FLAG_DISPLAY(TileFlags::IS_INTERACTING);
    FLAG_DISPLAY(TileFlags::IS_DAMAGED);
    FLAG_DISPLAY(TileFlags::HAS_ITEM_STACK);
    FLAG_DISPLAY(TileFlags::IS_RESOURCE_RESERVED);
    FLAG_DISPLAY(TileFlags::IN_CITY);

    static_assert(e_cast(TileFlags::TERM) == BIT(7), "Update");
}

inline void showTileLayerMainThread(const char* format, int layer, const TileHandle& tileHandle) {
    ui32 id;
    if (layer == 0) {
        id = tileHandle.getTile().getMainID();
    }
    else {
        id = tileHandle.getFloorTile();
    }
    if (id == TILE_ID_NONE) {
        ImGui::Text(format, id, "NONE");
    }
    else {
        ImGui::Text(format, id, TileRepository::get().getLoadedOrUnloadedAsset(id).displayName.c_str());
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
    const i32v3 dims = container.getTileSpatialGrid().getDims();
    const i32v3 xyzOffset = container.getTileSpatialGrid().getTileXYZOffset(mTileHandle.tileIndex);
    const i32v2 xyOffset(xyzOffset.x, xyzOffset.y);
    const i32v2 worldPos2D = xyOffset + i32v2(container.getWorldPos());
    bool isOwned = true;
    if (container.getOwnerType() == TileContainerOwnerType::BUILDING) {
        Building* owner = container.getOwnerBuilding();
        assert(owner);
        isOwned = owner->isTileOwned(mTileHandle.tileIndex);
    }

    // Debug tile cursor
    f32v3 worldPos3D = mTileHandle.getWorldPos3D();
    if (mTileHandle.getTile().getGroundZOffset()) {
        AM::DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), color4(255, 128, 128, 180));
        worldPos3D.z += mTileHandle.getTile().getGroundZOffset();
    }
    if (isOwned) {
        AM::DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), COLOR_WHITE);
    }
    else {
        AM::DebugRenderer::drawWireQuad(worldPos3D, f32v2(1.0f), COLOR_RED);
    }
    {
        constexpr f32 AXIS_THICKNESS = 0.1f;
        AM::DebugRenderer::drawLine(worldPos3D + f32v3(1.0f, 0.5f, 0.0f), f32v3(AXIS_THICKNESS, 0.0f, 0.0f), COLOR_RED);
        AM::DebugRenderer::drawLine(worldPos3D + f32v3(0.5f, 1.0f, 0.0f), f32v3(0.0f, AXIS_THICKNESS, 0.0f), COLOR_GREEN);
    }

    ImGui::Begin("Inspect Tile", nullptr, ImGuiWindowFlags_NoCollapse);
    if (!isOwned) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "===UNOWNED TILE===");
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
    ImGui::Text("Container Dims: <%u,%u,%u>", dims.x, dims.y, dims.z);

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
    showTileLayerMainThread("  Ground: %u %s", 1, mTileHandle);
    showTileLayerMainThread("  Main: %u %s", 0, mTileHandle);
    ImGui::Separator();
    showTileFlagsMainThread(mTileHandle);

    ImGui::End();
}
