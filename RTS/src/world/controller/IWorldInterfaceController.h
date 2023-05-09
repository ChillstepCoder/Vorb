#pragma once

class CameraController;
class IWorld;
DECL_VUI(class GameWindow);

// Handles input, UI, ect for a World
class IWorldInterfaceController
{
public:
    IWorldInterfaceController(vui::GameWindow& window, IWorld& world, CameraController& cameraController);
    virtual ~IWorldInterfaceController() {};
    virtual void update() = 0;
    virtual void renderUI() = 0;
    virtual void init() = 0;

protected:
    vui::GameWindow* mWindow = nullptr;
    IWorld* mWorld = nullptr;
    CameraController* mCameraController = nullptr;
};

