#include "stdafx.h"
#include "UIInteractMenuPopup.h"

#include "World.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

#include "DebugRenderer.h"


const ImVec2 sButtonSize(150, 25);
constexpr int WINDOW_FLAGS = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
//
//static bool view(NoiseFunction& n) {
//    bool changed = false;
//    ImGui::Text(n.label.c_str());
//    changed |= ImGui::SliderInt((n.label + "_octaves").c_str(), &n.octaves, 1, 15);
//    changed |= ImGui::SliderScalar((n.label + "_persist").c_str(), ImGuiDataType_Double, &n.persistence, &f64_zero, &f64_one);
//    changed |= ImGui::SliderScalar((n.label + "_freq").c_str(), ImGuiDataType_Double, &n.frequency, &f64_zero, &f64_one, "%.10f", ImGuiSliderFlags_Logarithmic);
//    changed |= ImGui::SliderScalarN((n.label + "_offset").c_str(), ImGuiDataType_Double, &(n.offset.x), 2, &MIN_NOISE_OFFSET, &MAX_NOISE_OFFSET, "%.10f");
//    return changed;
//}

UIInteractMenuPopup::UIInteractMenuPopup(const f32v2& screenPos, SDL_Window* window, WorldObjectQuery&& worldObjectQuery) :
    mScreenPos(screenPos),
    mWindow(window),
    mWorldObjectQuery(std::move(worldObjectQuery))
{

}

UIInteractMenuPopup::~UIInteractMenuPopup()
{

}

UIInteractMenuResultFlags UIInteractMenuPopup::updateAndRender()
{
    ui32 resultFlags;
    const f32v2 panelDims(sButtonSize.x + 16, INTERACT_MENU_RESULT_COUNT * sButtonSize.y + 45);
    const f32v2 clampedScreenPos = sMainGameWindowHandle->clampBoxPosToWindow(mScreenPos, panelDims);
    
    ImGui::SetNextWindowPos(ImVec2(clampedScreenPos.x, clampedScreenPos.y));
    ImGui::SetNextWindowSize(ImVec2(panelDims.x, panelDims.y));

    if (mWorldObjectQuery.getStructureTileHandle().isValid()) {
        resultFlags = updateAndRenderStructureTile();
    }
    else {
        resultFlags = updateAndRenderTerrainTile();
    }


    ImGui::End();
    // TODO: Why are these flags? Use bitflags?
    return static_cast<UIInteractMenuResultFlags>(resultFlags);
}

ui32 UIInteractMenuPopup::updateAndRenderTerrainTile() {
    ui32 resultFlags = 0;
    World& world = mWorldObjectQuery.getWorld();

    switch (mState) {
        case UIInteractMenuState::SELECT_OBJECT: {
            ImGui::Begin("Select Object", nullptr, WINDOW_FLAGS);

            int i = 1;
            int optionCount = 0;
            UIInteractMenuState nextState = UIInteractMenuState::SELECT_OBJECT; // Store best state in case we only have one so we can auto select

            // Agents
            for (auto& it : mWorldObjectQuery.getEntities()) {
                if (CharacterDetailsComponent* cmp = world.getECS().mRegistry.try_get<CharacterDetailsComponent>(it.second)) {
                    ++optionCount;
                    nextState = UIInteractMenuState::SELECTED_AGENT;
                    if (ImGui::Button((std::to_string(i++) + " " + cmp->name).c_str(), sButtonSize)) {
                        mState = nextState;
                        break;
                    }
                }
            }

            // Stockpile
            if (mWorldObjectQuery.getStockpile()) {
                ++optionCount;
                nextState = UIInteractMenuState::SELECTED_STOCKPILE;
                if (ImGui::Button((std::to_string(i++) + " Stockpile").c_str(), sButtonSize)) {
                    mState = nextState;
                    break;
                }
            }

            // Tile
            ++optionCount;
            nextState = UIInteractMenuState::SELECTED_TILE;
            if (ImGui::Button((std::to_string(i++) + " Tile").c_str(), sButtonSize)) {
                mState = nextState;
                break;
            }

            // Structure list
            ++optionCount;
            TileHandle handle = mWorldObjectQuery.getTileHandle();
            Chunk& chunk = world.getChunk(handle.getChunkIDAtPos());
            StructureArrayPtr structures = chunk.getStructuresAt(handle.tileIndex);
            if (structures.second) {
                nextState = UIInteractMenuState::SELECTED_STRUCTURE_LIST;
                if (ImGui::Button("Structures")) {
                    mState = nextState;
                    break;
                }
            }

            if (optionCount == 0) {
                resultFlags = INTERACT_MENU_RESULT_INVALID;
            }
            else if (optionCount == 1) {
                // Auto select
                mState = nextState;
            }

            break;
        }
        case UIInteractMenuState::SELECTED_TILE: {
            ImGui::Begin("Tile action", nullptr, WINDOW_FLAGS);

            if (ImGui::Button("Go Here", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_PATHFIND;
            }
            if (ImGui::Button("Inspect", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_INSPECT;
            }
            if (ImGui::Button("Clear Tile", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_CLEAR_TILE;
            }
            if (ImGui::Button("Plant Tree", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_PLANT_TREE;
            }
            if (ImGui::Button("Plant Pine Tree", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_PLANT_TREE_2;
            }
            if (ImGui::Button("Build Wall", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_BUILD_WALL;
            }
            break;
        }
        case UIInteractMenuState::SELECTED_STOCKPILE: {
            ImGui::Begin("Stockpile", nullptr, WINDOW_FLAGS);

            if (ImGui::Button("DEBUG: Add 25 wood", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD;
            }
            if (ImGui::Button("DEBUG: Destroy", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_DEBUG_DESTROY_STOCK;
            }
            break;
        }
        case UIInteractMenuState::SELECTED_AGENT: {
            ImGui::Begin("Agent", nullptr, WINDOW_FLAGS);

            if (ImGui::Button("DEBUG: Kill Agent", sButtonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_DEBUG_KILL_AGENT;
            }
            break;
        }
        case UIInteractMenuState::SELECTED_STRUCTURE_LIST: {
            ImGui::Begin("Structures", nullptr, WINDOW_FLAGS);
            TileHandle handle = mWorldObjectQuery.getTileHandle();
            Chunk& chunk = world.getChunk(handle.getChunkIDAtPos());
            StructureArrayPtr structures = chunk.getStructuresAt(handle.tileIndex);
            if (!structures.second) {
                // If we got here the structure  was deleted while we had it selected
                resultFlags = INTERACT_MENU_RESULT_INVALID;
            }
            else {
                for (ui16 i = 0; i < structures.second; ++i) {
                    Structure* structure = structures.first[i];
                    if (structure->getType() == StructureType::Building) {
                        nString name = "Building " + std::to_string(i);
                        if (ImGui::Button(name.c_str())) {
                            mState = UIInteractMenuState::SELECTED_STRUCTURE;
                            mSelectedStructure = structure;
                            break;
                        }
                    }
                }
            }
            break;
        }
        case UIInteractMenuState::SELECTED_STRUCTURE: {
            ImGui::Begin("Structure", nullptr, WINDOW_FLAGS);
            if (mSelectedStructure->getType() == StructureType::Building) {
                Building* building = static_cast<Building*>(mSelectedStructure);
                // TODO: Path to room
                ImGui::Text("Path to room");
                const std::vector<RoomNode>& roomGraph = building->getRooms();
                for (size_t i = 0; i < roomGraph.size(); ++i) {
                    const RoomNode& room = roomGraph[i];
                    if (ImGui::Button((room.roomDef->name + " " + std::to_string(i)).c_str())) {
                        resultFlags |= INTERACT_MENU_RESULT_DEBUG_PATH_ROOM;
                        mSelectedRoomID = room.id;
                        break;
                    }
                }
            }
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(INTERACT_MENU_RESULT_COUNT == 11, "update");
    static_assert(e_cast(UIInteractMenuState::COUNT) == 6, "update");    return resultFlags;
}

ui32 UIInteractMenuPopup::updateAndRenderStructureTile() {
    ui32 resultFlags = 0;
    ImGui::Begin("Structure", nullptr, WINDOW_FLAGS);
    if (ImGui::Button("Move Here")) {
        resultFlags |= INTERACT_MENU_RESULT_DEBUG_PATH_ROOM;
        mSelectedTileHandle = mWorldObjectQuery.getStructureTileHandle();
    }
    DebugRenderer::drawWireQuad(f32v3(mWorldObjectQuery.getStructureTileHandle().getWorldPos3D()), f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 1.0f));
    return resultFlags;
}

const RoomNode* UIInteractMenuPopup::tryGetSelectedRoom() const {
    if (!mSelectedStructure || mSelectedRoomID == INVALID_ROOM_ID) {
        return nullptr;
    }
    assert(mSelectedStructure->getType() == StructureType::Building);

    Building* building = static_cast<Building*>(mSelectedStructure);
    auto&& roomGraph = building->getRooms();
    assert(mSelectedRoomID < roomGraph.size());
    return &roomGraph[mSelectedRoomID];
}

Building* UIInteractMenuPopup::tryGetSelectedBuilding() const {
    if (mSelectedStructure && mSelectedStructure->getType() == StructureType::Building) {
        return static_cast<Building*>(mSelectedStructure);
    }
    return nullptr;
}
