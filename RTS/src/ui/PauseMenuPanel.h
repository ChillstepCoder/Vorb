#pragma once

enum class PauseMenuPanelResult {
    NONE,
    RESUME,
    EXIT_TO_MENU,
    EXIT_TO_DESKTOP
};

class PauseMenuPanel
{
public:
    PauseMenuPanelResult updateAndRender();
};

