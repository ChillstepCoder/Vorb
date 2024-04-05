#pragma once

struct SDL_Window;
class Building;
struct RoomGenNode;
class Building;
class World;

#include "city/CityConst.h"
#include "world/WorldObjectQuery.h"

enum UIInteractMenuResultFlags : ui32 {
    INTERACT_MENU_RESULT_PATHFIND          = 1 << 0,
    INTERACT_MENU_RESULT_INSPECT           = 1 << 1,
    INTERACT_MENU_RESULT_CLEAR_TILE        = 1 << 2,
    INTERACT_MENU_RESULT_PLANT_TREE        = 1 << 3,
    INTERACT_MENU_RESULT_PLANT_TREE_2      = 1 << 4,
    INTERACT_MENU_RESULT_BUILD_WALL        = 1 << 5,
    INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD = 1 << 6,
    INTERACT_MENU_RESULT_DEBUG_DESTROY_STOCK = 1 << 7,
    INTERACT_MENU_RESULT_DEBUG_KILL_AGENT  = 1 << 8,
    INTERACT_MENU_RESULT_INVALID           = 1 << 9,
    INTERACT_MENU_RESULT_DEBUG_NAVMESH     = 1 << 10,
    INTERACT_MENU_RESULT_DEBUG_FINE_NAVMESH = 1 << 11,
    INTERACT_MENU_RESULT_DEBUG_NAV_NODE = 1 << 12,
    INTERACT_MENU_RESULT_DEBUG_HARVESTABLES = 1 << 13,
    INTERACT_MENU_RESULT_DEBUG_PATH_TO_WOOD = 1 << 14,
    INTERACT_MENU_RESULT_REBUILD_NAVMESH    = 1 << 15,
    INTERACT_MENU_RESULT_COUNT               = 16
};

enum class UIInteractMenuState {
    SELECT_OBJECT,
    SELECTED_TILE,
    SELECTED_STOCKPILE,
    SELECTED_AGENT,
    SELECTED_STRUCTURE_LIST,
    COUNT
};

// Right click interact menu
class TileInteractPanel
{
public:
    TileInteractPanel(World& world, const f32v2& screenPos, SDL_Window* window, const WorldObjectQueryPtr& worldObjectQuery);
    ~TileInteractPanel();

    UIInteractMenuResultFlags updateAndRender();


    WorldObjectQueryPtr& getWorldObjects() { return mWorldObjectQuery; }

    Building* getSelectedStructure() const { return mSelectedStructure; }
    RoomNodeID getSelectedRoomID() const { return mSelectedRoomID; }
    const RoomGenNode* tryGetSelectedRoom() const;
    Building* tryGetSelectedBuilding() const;
    TileHandle getSelectedTileHandle() const { return mSelectedTileHandle; }

private:
    ui32 updateAndRenderTerrainTile();
    ui32 updateAndRenderStructureTile();
    const ui32v2 mScreenPos;
    SDL_Window* mWindow;
    WorldObjectQueryPtr mWorldObjectQuery;
    UIInteractMenuState mState = UIInteractMenuState::SELECT_OBJECT;
    Building* mSelectedStructure = nullptr;
    RoomNodeID mSelectedRoomID = INVALID_ROOM_ID;
    TileHandle mSelectedTileHandle;
    World& mWorld;
};

