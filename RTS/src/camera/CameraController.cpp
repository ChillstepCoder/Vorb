#include "stdafx.h"
#include "CameraController.h"

#include "options/DebugOptions.h"

#include "world/IWorld.h"
#include "physics/PhysicsWorld.h"

#include "ecs/IEntityComponentSystem.h"
#include <Vorb/ui/GameWindow.h>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/GameTime.h>

const f32v2 CAMERA_ZOOM_RANGE = f32v2(1.0f, 1024.0f);

CameraController::CameraController(vui::GameWindow& window) : mWindow(window) {

    // Init listeners
    vui::InputDispatcher::key.registerKeyListeners(mKeyListeners);
    vui::InputDispatcher::mouse.registerMouseListeners(mMouseListeners);


    mCamera.init((f32)window.getWidth() / window.getHeight());
    setCameraMode(sDebugOptions.mCameraMode);
   
}

void CameraController::update(f32 deltaTime, f32 frameAlpha, const f32v3& followEntityPos) {
    PROFILE_FUNCTION();
    if (mCameraMode != sDebugOptions.mCameraMode) {
        setCameraMode(sDebugOptions.mCameraMode);
    }

    // Update any changed options
    if (mCamera.getFieldOfView() != sDebugOptions.mFoV) {
        mCamera.setFieldOfView(sDebugOptions.mFoV);
    }

    // Editor mode is an override
    if (mEditorMode) {
        updateCameraEditorMode(frameAlpha, followEntityPos);
    }
    else {
        switch (mCameraMode) {
            case CameraMode::CARTESIAN:
                updateCameraCartesianMode(frameAlpha, followEntityPos);
                break;
            case CameraMode::MMO:
                updateCameraMMOMode(frameAlpha, followEntityPos);
                break;
            case CameraMode::MOUSELOCK:
                break;
            case CameraMode::FREE_LOOK:
                updateCameraFreeLookMode(frameAlpha, deltaTime);
                break;
            case CameraMode::FIRST_PERSON:
                updateCameraFirstPersonMode(frameAlpha, followEntityPos);
                break;
            default:
                break;
        }
    }
    static_assert(e_cast(CameraMode::COUNT) == 6, "Add new mode functionality");

    // Update camera itself
    mCamera.update();
}

void CameraController::setCameraMode(CameraMode cameraMode) {
    if (mCameraMode == cameraMode) {
        return;
    }

    // Remove any old listeners
    mKeyListeners.reset();
    mMouseListeners.reset();

    // Switch our camera mode
    mCameraMode = cameraMode;
    // Add new inputs
    switch (mCameraMode) {
        case CameraMode::CARTESIAN:
            vui::InputDispatcher::mouse.addWheelListener(mMouseListeners, [this](const vui::MouseWheelEvent& e) { updateMouseWheelInput(e); });
            vui::InputDispatcher::key.addKeyDownListener(mKeyListeners, [this](const vui::KeyEvent& e) { updateKeyInputCartesianMode(e); });
            mIsMouseHidden = false;
            break;
        case CameraMode::MMO:
            vui::InputDispatcher::mouse.addWheelListener(mMouseListeners, [this](const vui::MouseWheelEvent& e) { updateMouseWheelInputMMOMode(e); });
            vui::InputDispatcher::mouse.addMotionListener(mMouseListeners, [this](const vui::MouseMotionEvent& e) { updateMouseMotionInputMMOMode(e); });
            vui::InputDispatcher::mouse.addButtonDownListener(mMouseListeners, [this](const vui::MouseButtonEvent& e) { updateMouseButtonDownInputMMO(e); });
            vui::InputDispatcher::mouse.addButtonUpListener(mMouseListeners, [this](const vui::MouseButtonEvent& e) { updateMouseButtonUpInputMMO(e); });
            mIsMouseHidden = false;
            break;
        case CameraMode::MOUSELOCK:
            vui::InputDispatcher::mouse.addWheelListener(mMouseListeners, [this](const vui::MouseWheelEvent& e) { updateMouseWheelInput(e); });
            mIsMouseHidden = false;
            break;
        case CameraMode::FREE_LOOK:
            vui::InputDispatcher::mouse.addMotionListener(mMouseListeners, [this](const vui::MouseMotionEvent& e) { updateMouseMotionInputFreeLookMode(e); });
            mIsMouseHidden = false;
            break;
        case CameraMode::FIRST_PERSON:
            vui::InputDispatcher::mouse.addMotionListener(mMouseListeners, [this](const vui::MouseMotionEvent& e) { updateMouseMotionInputFirstPersonMode(e); });
            mIsMouseHidden = true;
            mLastMousePositionBeforeRelative = i32v2(mWindow.getWidth() * 0.5f, mWindow.getHeight() * 0.5f);
            break;
        case CameraMode::NONE:
        default:
            break;
    }
    static_assert(e_cast(CameraMode::COUNT) == 6, "Update any input register");

}

void CameraController::setCameraDirection(const f32v3& dir) {
    mCameraDirectionTweener.mTarget = dir;
}

void CameraController::updateCameraCartesianMode(f32 frameAlpha, const f32v3& ownerEntityPos) {

    // TODO: Delta time dependent?

    f32v3 followTargetPos = ownerEntityPos;

    // Camera follow
    constexpr float MAX_SPEED_MPS = 0.3f;
    const f32 maxSpeed = MAX_SPEED_MPS * mCameraPositionTweener.mCurr.z;
    f32v3 targetPos(followTargetPos.x, followTargetPos.y, mCameraPositionTweener.mTarget.z);
    mCameraPositionTweener.setTarget(targetPos);
    mCameraPositionTweener.setMaxSpeed(MAX_SPEED_MPS * mCameraPositionTweener.mCurr.z);

    mCameraPositionTweener.update(1.0f);
    mCameraDirectionTweener.update(1.0f);

    const f32v3 lookAtOffset(mCameraDirectionTweener.mCurr.x * sDebugOptions.mCameraXYDistance, mCameraDirectionTweener.mCurr.y * sDebugOptions.mCameraXYDistance, mCameraDirectionZOffset * sDebugOptions.mCameraZHeight);
    mCamera.lookAt(mCamera.getPosition() + lookAtOffset);

    // Position tweener causes juttering
    //mCamera3D->setPosition(mCameraPositionTweener.mCurr - lookAtOffset * mCameraPositionTweener.mCurr.z + f32v3(0.0f, 0.0f, playerZPos));
    mCamera.setPosition(targetPos - lookAtOffset * mCameraPositionTweener.mTarget.z + f32v3(0.0f, 0.0f, followTargetPos.z));

    // Increase Z clip as camera goes higher to reduce precision issues and make fog move away from camera
    /*const f32 zNearAlpha = glm::clamp(mCamera.getPosition().z * 0.001f, 0.0f, 1.0f);
    const f32 zNear = lerp(0.1f, 5.0f, zNearAlpha);
    mCamera.setClippingPlane(zNear, sDebugOptions.mZFar);*/

}

void CameraController::updateCameraFreeLookMode(f32 frameAlpha, f32 deltaTime) {

    f32 cameraSpeed = 0.1f * deltaTime;
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
        cameraSpeed *= 35.0f;
    }

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_W)) {
        mCamera.offsetPosition(mCamera.getFrontVector() * cameraSpeed);
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_S)) {
        mCamera.offsetPosition(-mCamera.getFrontVector() * cameraSpeed);
    }

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
        mCamera.offsetPosition(-mCamera.getRightVector() * cameraSpeed);
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_D)) {
        mCamera.offsetPosition(mCamera.getRightVector() * cameraSpeed);
    }

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
        mCamera.offsetPosition(mCamera.getUpVector() * cameraSpeed);
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL)) {
        mCamera.offsetPosition(-mCamera.getUpVector() * cameraSpeed);
    }
}

void CameraController::updateCameraMMOMode(f32 frameAlpha, const f32v3& ownerEntityPos)
{

    mCameraBoomLengthTweener.update(1.0f); // TODO: Use deltatime

    // TODO: Delta time dependent?

    f32v3 followTargetPos = ownerEntityPos;
    followTargetPos.z += sDebugOptions.mCameraZHeight + SQ(mCameraBoomLengthTweener.getCurr() * 0.5f);

    const f32v3 lookAtOffset = mCamera.getDirection() * sDebugOptions.mCameraXYDistance * 2.0f * mCameraBoomLengthTweener.getCurr();
    const f32v3 camPos = followTargetPos - lookAtOffset;

    // Collision raycast
    //PhysHitResult result;
    // TODO: We used to use tryPick here but it causes contention with the physics system and jitters
    // TODO: Transparent render all objects?
    //result = sWorld->getPhysicsWorld().pick(followTargetPos, camPos, PICK_TYPE_STATIC);
    // DebugRenderer::drawWireQuad(followTargetPos, f32v2(0.2f), COLOR_WHITE);
    //if (result.didHit()) {
    //    mCamera.setPosition(result.mPosition);
    //    mCamera.lookAt(followTargetPos);
    //}
    //else {
        mCamera.setPosition(camPos);
        mCamera.lookAt(followTargetPos);
    //}

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_ESCAPE)) {
        mIsMouseHidden = false;
    }

    if (mIsMouseHidden != mWasMouseHidden) {
        if (mIsMouseHidden) {
            mWindow.setRelativeMouseMode(true);
        } else {
            mWindow.setRelativeMouseMode(false);
            mWindow.warpMouse(mLastMousePositionBeforeRelative.x, mLastMousePositionBeforeRelative.y);
        }
        mWasMouseHidden = mIsMouseHidden;
    }

    // Increase Z clip as camera goes higher to reduce precision issues and make fog move away from camera
  /*  const f32 zNearAlpha = glm::clamp(mCamera.getPosition().z * 0.001f, 0.0f, 1.0f);
    const f32 zNear = lerp(0.1f, 5.0f, zNearAlpha);
    mCamera.setClippingPlane(zNear, sDebugOptions.mZFar);*/
}

// TODO: Use?
void CameraController::updateCameraEditorMode(f32 frameAlpha, const f32v3& ownerEntityPos)
{
    if (mIsMouseHidden) {
        mIsMouseHidden = false;
        mWindow.setRelativeMouseMode(false);
        mWasMouseHidden = mIsMouseHidden;
    }

    mCamera.setPosition(ownerEntityPos);
    mCamera.lookAt(ownerEntityPos + mCameraDirectionTweener.mTarget);
}

void CameraController::updateCameraFirstPersonMode(f32 frameAlpha, const f32v3& ownerEntityPos) {
    const f32v3 followTargetPos = ownerEntityPos;
    mCamera.setPosition(followTargetPos + f32v3(0.0f, 0.0f, sDebugOptions.mCameraZHeight));

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_ESCAPE)) {
        mIsMouseHidden = false;
    }

    if (mIsMouseHidden != mWasMouseHidden) {
        if (mIsMouseHidden) {
            mWindow.setRelativeMouseMode(true);
        }
        else {
            mWindow.setRelativeMouseMode(false);
            mWindow.warpMouse(mLastMousePositionBeforeRelative.x, mLastMousePositionBeforeRelative.y);
        }
        mWasMouseHidden = mIsMouseHidden;
    }
}


void CameraController::updateMouseWheelInput(const vui::MouseWheelEvent& evnt) {
    mCameraPositionTweener.mTarget.z = glm::clamp(mCameraPositionTweener.mTarget.z + evnt.dy * mCameraPositionTweener.mTarget.z * -0.2f, CAMERA_ZOOM_RANGE.x, CAMERA_ZOOM_RANGE.y);
}

void CameraController::updateMouseWheelInputMMOMode(const vui::MouseWheelEvent& evnt) {
    mCameraBoomLengthTweener.mTarget = glm::clamp(mCameraBoomLengthTweener.mTarget + (f32)evnt.dy * mCameraBoomLengthTweener.mTarget * -0.2f, 0.5f, 20.0f /*3.0f*/);
}

void CameraController::updateMouseMotionInputFreeLookMode(const vui::MouseMotionEvent& evnt) {
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::RIGHT)) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, -evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateMouseMotionInputMMOMode(const vui::MouseMotionEvent& evnt) {
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::RIGHT)) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, -evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateMouseMotionInputFirstPersonMode(const vui::MouseMotionEvent& evnt) {
    if (mIsMouseHidden) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, -evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateKeyInputCartesianMode(const vui::KeyEvent& evnt) {
    if (evnt.keyCode == VKEY_Q) {
        mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][1];
        mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
    }
    else if (evnt.keyCode == VKEY_E) {
        mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][0];
        mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
    }
}

void CameraController::updateMouseButtonDownInputMMO(const vui::MouseButtonEvent& evnt) {
    // We cant set SDL mouse mode inside the event handler or it causes bugs
    if (evnt.button == vorb::ui::MouseButton::RIGHT) {
        mIsMouseHidden = true;
        mLastMousePositionBeforeRelative = i32v2(evnt.x, evnt.y);
    }
}

void CameraController::updateMouseButtonUpInputMMO(const vui::MouseButtonEvent& evnt) {
    if (evnt.button == vorb::ui::MouseButton::RIGHT) {
        mIsMouseHidden = false;
    }
}
