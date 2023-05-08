#pragma once

class CameraController;
class IWorld;
DECL_VUI(class GameWindow);

// Handles input, UI, ect for a World
class IWorldInterfaceController
{
public:
    virtual void init(vui::GameWindow& window, IWorld& world, CameraController& cameraController) = 0;
    virtual void update() = 0;
    virtual void renderUI() = 0;

};

