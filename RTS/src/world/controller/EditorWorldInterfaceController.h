#pragma once

#include <Vorb/ui/KeyboardEventManager.h>
#include <Vorb/ui/MouseEventManager.h>

#include "IWorldInterfaceController.h"

#include "tile/TileHandle.h"
#include "world/WorldObjectQuery.h"

class TileInteractPanel;
class DeferredPhysicsPick;

class EditorWorldInterfaceController : public IWorldInterfaceController
{
public:
    void init(vui::GameWindow& window, IWorld& world, CameraController& cameraController) override;
    void update() override;
    void renderUI() override;

private:
    void updateTilePicking();
    void initEvents();
    void tryUpdateAndRenderInteractPopup();

    // Pathfinding test
    ui32v2 mPathFindStart = ui32v2(0);
    bool mIsPathfinding = false;

    vui::GameWindow* mWindow = nullptr;
    IWorld* mWorld = nullptr;
    CameraController* mCameraController = nullptr;
    // UI
    TileHandle mSelectedTileHandle;
    f32v2 mSelectedScreenPos = f32v2(0.0f);
    std::unique_ptr<TileInteractPanel> mRightClickInteractPopup;
    f32v2 mMousePosition = f32v2(0.0f);
    PreciseTimer mRightClickTimer;
    f32v3 mRightClickPickPos = f32v3(FLT_MAX);
    f32v3 mMousePickRay = f32v3(0.0f);
    WorldObjectQueryPtr mWorldObjectQuery;
    bool mIsQuerying = false;
    std::unique_ptr<DeferredPhysicsPick> mRightClickDownPick;
    std::unique_ptr<DeferredPhysicsPick> mRightClickUpPick;
    f32v2 mRightClickUpPickScreenPos = f32v2(0.0);

    // Event listeners
    vui::MouseListeners mMouseListeners;
    vui::KeyListeners mKeyListeners;
};

