#pragma once

#include "FeatureConst.h"

class Camera2D;
class World;
struct SDL_Window;

#if IS_ENABLED(FEATURE_WORLD_EDITOR)

class WorldEditor
{
public:
    WorldEditor(World& world, SDL_Window* window, Camera2D& camera);

    // Setup inputs and whatever else
    void init();

    // Call in render method
    void updateAndRender();
private:
    void updateCamera();

    World& mWorld;
    Camera2D& mCamera;
    SDL_Window* mWindow;
        
    float mTargetScale = 50.0f;
    float mScale = 50.0f;
};

#endif