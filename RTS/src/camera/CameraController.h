#pragma once

#include "camera/CameraMode.h"
#include "camera/Camera3D.h"
#include <util/Tweener.h>

DECL_VUI(struct GameTime);
DECL_VUI(class GameWindow);
DECL_VUI(struct MouseWheelEvent);
DECL_VUI(struct MouseMotionEvent);
DECL_VUI(struct MouseButtonEvent);
DECL_VUI(struct KeyEvent);

class World;

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
    CameraController(vui::GameWindow& window, const World& world);

    void update(const vui::GameTime& gameTime, f32 frameAlpha);


    Camera3D& getOwnedCamera() { return mCamera; }
    void setEntityFollow(entt::entity followEntity);
    void setCameraMode(CameraMode cameraMode);
private:
    // Mode updates
    void updateCameraCartesianMode(f32 frameAlpha);
    void updateCameraFreeLookMode(f32 frameAlpha, f32 deltaTime);
    void updateCameraMMOMode(f32 frameAlpha);

    // Delegates
    void updateMouseWheelInput(Sender s, const vui::MouseWheelEvent& evnt);
    void updateMouseWheelInputMMOMode(Sender s, const vui::MouseWheelEvent& evnt);
    void updateMouseMotionInputFreeLookMode(Sender s, const vui::MouseMotionEvent& evnt);
    void updateMouseMotionInputMMOMode(Sender s, const vui::MouseMotionEvent& evnt);
    void updateKeyInputCartesianMode(Sender sender, const vui::KeyEvent& evnt);
    void updateMouseButtonDownInputMMO(Sender s, const vui::MouseButtonEvent& evnt);
    void updateMouseButtonUpInputMMO(Sender s, const vui::MouseButtonEvent& evnt);

    // Helpers
    f32v3 getFollowTargetPos(f32 frameAlpha);

    // Data
    Camera3D mCamera;
    CameraMode mCameraMode;

    vui::GameWindow& mWindow;
    const World& mWorld;

    entt::entity mEntityFollow = entt::null; // TODO: Need an event for entity destroy
    Cartesian mCameraCartesianDirection = Cartesian::UP;
    Tweener<f32v3> mCameraPositionTweener = Tweener<f32v3>(f32v3(0.0f));
    Tweener<f32> mCameraBoomLengthTweener = Tweener<f32>(1.0f);
    SphericalTweener<f32v3> mCameraDirectionTweener = SphericalTweener<f32v3>(TARGET_CAMERA_NORMALS_3D[e_cast(Cartesian::UP)], 0.4f/*speed*/, 0.2f/*acceleration*/);
    f32 mCameraDirectionZOffset = -0.3f;
    i32v2 mLastMousePositionBeforeRelative;
};

