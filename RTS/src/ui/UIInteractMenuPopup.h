#pragma once

struct SDL_Window;

#include "world/WorldObjectQuery.h"

enum UIInteractMenuResultFlags : ui32 {
    INTERACT_MENU_RESULT_PATHFIND          = 1 << 0,
    INTERACT_MENU_RESULT_INSPECT           = 1 << 1,
    INTERACT_MENU_RESULT_CLEAR_TILE        = 1 << 2,
    INTERACT_MENU_RESULT_PLANT_TREE        = 1 << 3,
    INTERACT_MENU_RESULT_BUILD_WALL        = 1 << 4,
    INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD = 1 << 5,
    INTERACT_MENU_RESULT_DEBUG_KILL_AGENT  = 1 << 6,
    INTERACT_MENU_RESULT_INVALID           = 1 << 7,
    INTERACT_MENU_RESULT_COUNT             = 8
};

enum class UIInteractMenuState {
    SELECT_OBJECT,
    SELECTED_TILE,
    SELECTED_STOCKPILE,
    SELECTED_AGENT,
    COUNT
};

// Right click interact menu
class UIInteractMenuPopup
{
public:
    UIInteractMenuPopup(const f32v2& screenPos, SDL_Window* window, WorldObjectQuery&& worldObjectQuery);
    ~UIInteractMenuPopup();

    UIInteractMenuResultFlags updateAndRender();

    WorldObjectQuery& getWorldObjects() { return mWorldObjectQuery; }

private:
    const ui32v2 mScreenPos;
    SDL_Window* mWindow;
    WorldObjectQuery mWorldObjectQuery;
    UIInteractMenuState mState = UIInteractMenuState::SELECT_OBJECT;
};

