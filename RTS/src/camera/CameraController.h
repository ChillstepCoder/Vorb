#pragma once

#include "camera/CameraMode.h"
#include "camera/Camera3D.h"
#include <util/Tweener.h>

#include "input/InputDispatcher.h"

DECL_VUI(struct GameTime);
DECL_VUI(class GameWindow);
DECL_VUI(struct MouseWheelEvent);
DECL_VUI(struct MouseMotionEvent);
DECL_VUI(struct MouseButtonEvent);
DECL_VUI(struct KeyEvent);

#define TARGET_CAMERA_OFFSET_XY 1.0f
const f32v2 TARGET_CAMERA_NORMALS_2D[4] = {
    glm::normalize(f32v2(0.0f, -TARGET_CAMERA_OFFSET_XY)), // Cartesian::DOWN
    glm::normalize(f32v2(-TARGET_CAMERA_OFFSET_XY, 0.0f)), // Cartesian::LEFT
    glm::normalize(f32v2(TARGET_CAMERA_OFFSET_XY,  0.0f)), // Cartesian::RIGHT
    glm::normalize(f32v2(0.0f, TARGET_CAMERA_OFFSET_XY))  // Cartesian::UP
};
const f32v3 TARGET_CAMERA_NORMALS_3D[4] = {
    glm::normalize(f32v3(0.0f, -TARGET_CAMERA_OFFSET_XY, 0.0f)), // Cartesian::DOWN
    glm::normalize(f32v3(-TARGET_CAMERA_OFFSET_XY, 0.0f, 0.0f)), // Cartesian::LEFT
    glm::normalize(f32v3(TARGET_CAMERA_OFFSET_XY,  0.0f, 0.0f)), // Cartesian::RIGHT
    glm::normalize(f32v3(0.0f, TARGET_CAMERA_OFFSET_XY, 0.0f))  // Cartesian::UP
};

// Owns and controls a 3D camera
// TODO: Separate camera updator class per type with an abstract base class
class CameraController {
public:
    CameraController(vui::GameWindow& window);

    void update(f32 deltaTime, f32 frameAlpha, const f32v3& ownerEntityPos);


    Camera3D& getOwnedCamera() { return mCamera; }
    const Camera3D& getOwnedCamera() const { return mCamera; }
    void setCameraMode(CameraMode cameraMode);

    void setCameraDirection(const f32v3& dir);
    void setEditorMode(bool editorMode) { mEditorMode = editorMode; }
private:
    // Mode updates
    void updateCameraFreeLookMode(f32 frameAlpha, f32 deltaTime);
    void updateCameraMMOMode(f32 frameAlpha, const f32v3& ownerEntityPos);
    void updateCameraEditorMode(f32 frameAlpha, const f32v3& ownerEntityPos);
    void updateCameraFirstPersonMode(f32 frameAlpha, const f32v3& ownerEntityPos);

    // Delegates
    void updateMouseWheelInput(const vui::MouseWheelEvent& evnt);
    void updateMouseWheelInputMMOMode(const vui::MouseWheelEvent& evnt);
    void updateMouseMotionInputFreeLookMode(const vui::MouseMotionEvent& evnt);
    void updateMouseMotionInputMMOMode(const vui::MouseMotionEvent& evnt);
    void updateMouseMotionInputFirstPersonMode(const vui::MouseMotionEvent& evnt);
    void updateKeyInputCartesianMode(const vui::KeyEvent& evnt);
    void updateMouseButtonDownInputMMO(const vui::MouseButtonEvent& evnt);
    void updateMouseButtonUpInputMMO(const vui::MouseButtonEvent& evnt);

    // Data
    Camera3D mCamera;
    CameraMode mCameraMode;

    vui::GameWindow& mWindow;
    bool mWasMouseHidden = false;
    bool mIsMouseHidden = false;
    bool mEditorMode = false;

    Cartesian mCameraCartesianDirection = Cartesian::NORTH;
    Tweener<f32v3> mCameraPositionTweener = Tweener<f32v3>(f32v3(0.0f));
    Tweener<f32> mCameraBoomLengthTweener = Tweener<f32>(1.0f);
    SphericalTweener<f32v3> mCameraDirectionTweener = SphericalTweener<f32v3>(TARGET_CAMERA_NORMALS_3D[e_cast(Cartesian::NORTH)], 0.4f/*speed*/, 0.2f/*acceleration*/);
    f32 mCameraDirectionZOffset = -0.3f;
    i32v2 mLastMousePositionBeforeRelative;

    // Event handles
    vui::MouseListeners mMouseListeners;
    vui::KeyListeners mKeyListeners;
};

