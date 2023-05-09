#pragma once

#include <Vorb/ui/KeyboardEventManager.h>
#include <Vorb/ui/MouseEventManager.h>

#include "IWorldInterfaceController.h"

#include "tile/TileHandle.h"
#include "world/WorldObjectQuery.h"

// TODO: Why doesn't forward declare work?
#include "physics/PhysHitResult.h"
#include "ui/TileInteractPanel.h"

class EditorWorldInterfaceController : public IWorldInterfaceController
{
public:
    EditorWorldInterfaceController(vui::GameWindow& window, IWorld& world, CameraController& cameraController) : IWorldInterfaceController(window, world, cameraController) {};
    virtual ~EditorWorldInterfaceController();
    void update() override;
    void renderUI() override;
    void init() override;

    void setMousePosition(const f32v2& mousePosition) { mMousePosition = mousePosition; }
protected:
    void updateTilePicking();
    void initEvents();
    void tryUpdateAndRenderInteractPopup();

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

