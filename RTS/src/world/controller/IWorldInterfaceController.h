#pragma once

class CameraController;
class World;
DECL_VUI(class GameWindow);

// Handles input, UI, ect for a World
class IWorldInterfaceController
{
public:
    IWorldInterfaceController(vui::GameWindow& window, World& world, CameraController& cameraController);
    virtual ~IWorldInterfaceController() {};
    virtual void update() = 0;
    virtual void renderUI() = 0;
    virtual void init() = 0;

    vui::GameWindow* getGameWindow() const { return mWindow; }
    World* getWorld() const { return mWorld; }
    CameraController* getCameraController() const { return mCameraController; }
protected:
    vui::GameWindow* mWindow = nullptr;
    World* mWorld = nullptr;
    CameraController* mCameraController = nullptr;
};

