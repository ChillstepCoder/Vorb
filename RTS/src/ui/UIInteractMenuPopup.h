#pragma once


struct SDL_Window;

enum UIInteractMenuResultFlags : ui32 {
    INTERACT_MENU_RESULT_PATHFIND   = 1 << 0,
    INTERACT_MENU_RESULT_INSPECT    = 1 << 1,
    INTERACT_MENU_RESULT_CLEAR_TILE = 1 << 2,
    INTERACT_MENU_RESULT_PLANT_TREE = 1 << 3,
    INTERACT_MENU_RESULT_BUILD_WALL = 1 << 4,
    INTERACT_MENU_RESULT_COUNT      = 5
};


// Right click interact menu
class UIInteractMenuPopup
{
public:
    UIInteractMenuPopup(const f32v2& screenPos, SDL_Window* window);
    ~UIInteractMenuPopup();

    UIInteractMenuResultFlags updateAndRender();

private:
    const ui32v2 mScreenPos;
    SDL_Window* mWindow;
};

