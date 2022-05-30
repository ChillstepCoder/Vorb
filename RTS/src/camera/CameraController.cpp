#include "stdafx.h"
#include "CameraController.h"

#include "options/DebugOptions.h"

#include "World.h"

#include "ecs/EntityComponentSystem.h"
#include <Vorb/ui/GameWindow.h>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/ui/GameTime.h>

const f32v2 CAMERA_ZOOM_RANGE = f32v2(1.0f, 1024.0f);

CameraController::CameraController(vui::GameWindow& window, const World& world) : mWindow(window), mWorld(world) {

    mCamera.init((f32)window.getWidth() / window.getHeight());
    setCameraMode(sDebugOptions.mCameraMode);
   
}

void CameraController::update(const vui::GameTime& gameTime, f32 frameAlpha) {

    if (mCameraMode != sDebugOptions.mCameraMode) {
        setCameraMode(sDebugOptions.mCameraMode);
    }

    // Currently unsupported
    if (mEntityFollow == entt::null) {
        return;
    }

    // Update any changed options
    if (mCamera.getFieldOfView() != sDebugOptions.mFoV) {
        mCamera.setFieldOfView(sDebugOptions.mFoV);
    }

    switch (mCameraMode) {
        case CameraMode::CARTESIAN:
            updateCameraCartesianMode(frameAlpha);
            break;
        case CameraMode::MMO:
            updateCameraMMOMode(frameAlpha);
            break;
        case CameraMode::MOUSELOCK:
            break;
        case CameraMode::FREE_LOOK:
            updateCameraFreeLookMode(frameAlpha, gameTime.deltaTime);
            break;
        case CameraMode::FIRST_PERSON:
            updateCameraFirstPersonMode(frameAlpha);
            break;
        default:
            break;
    }
    static_assert(e_cast(CameraMode::COUNT) == 6, "Add new mode functionality");

    // Update camera itself
    mCamera.update();
}

void CameraController::setEntityFollow(entt::entity followEntity) {
    if (mEntityFollow == entt::null) {
        auto& ecs = mWorld.getECS();
        const auto& physCmp = ecs.mRegistry.get<PhysicsComponent>(followEntity);
        //mCamera3D->setPosition(f32v3(WorldData::WORLD_CENTER.x, 2.0f, WorldData::WORLD_CENTER.y));
        mCamera.setPosition(physCmp.getPosition());
        mCameraPositionTweener = f32v3(WorldData::WORLD_CENTER.x, WorldData::WORLD_CENTER.y, 5.0f);
    }
    mEntityFollow = followEntity;
}

void CameraController::setCameraMode(CameraMode cameraMode) {
    if (mCameraMode == cameraMode) {
        return;
    }

    // Update inputs
    // Remove old inputs
    switch (mCameraMode) {
        case CameraMode::CARTESIAN:
            vui::InputDispatcher::mouse.onWheel -= makeDelegate(this, &CameraController::updateMouseWheelInput);
            vui::InputDispatcher::key.onKeyDown -= makeDelegate(this, &CameraController::updateKeyInputCartesianMode);
            break;
        case CameraMode::MMO:
            vui::InputDispatcher::mouse.onWheel -= makeDelegate(this, &CameraController::updateMouseWheelInputMMOMode);
            vui::InputDispatcher::mouse.onMotion -= makeDelegate(this, &CameraController::updateMouseMotionInputMMOMode);
            vui::InputDispatcher::mouse.onButtonDown -= makeDelegate(this, &CameraController::updateMouseButtonDownInputMMO);
            vui::InputDispatcher::mouse.onButtonUp -= makeDelegate(this, &CameraController::updateMouseButtonUpInputMMO);
            mWindow.setRelativeMouseMode(false);
            mIsMouseHidden = false;
            break;
        case CameraMode::MOUSELOCK:
            vui::InputDispatcher::mouse.onWheel -= makeDelegate(this, &CameraController::updateMouseWheelInput);
            break;
        case CameraMode::FREE_LOOK:
            vui::InputDispatcher::mouse.onMotion -= makeDelegate(this, &CameraController::updateMouseMotionInputFreeLookMode);
            break;
        case CameraMode::FIRST_PERSON:
            vui::InputDispatcher::mouse.onMotion -= makeDelegate(this, &CameraController::updateMouseMotionInputFirstPersonMode);
            mWindow.setRelativeMouseMode(false);
            mIsMouseHidden = false;
            break;
        case CameraMode::NONE:
        default:
            break;
    }
    static_assert(e_cast(CameraMode::COUNT) == 6, "Update any input unregister");

    // Switch our camera mode
    mCameraMode = cameraMode;
    // Add new inputs
    switch (mCameraMode) {
        case CameraMode::CARTESIAN:
            vui::InputDispatcher::mouse.onWheel += makeDelegate(this, &CameraController::updateMouseWheelInput);
            vui::InputDispatcher::key.onKeyDown += makeDelegate(this, &CameraController::updateKeyInputCartesianMode);
            break;
        case CameraMode::MMO:
            vui::InputDispatcher::mouse.onWheel += makeDelegate(this, &CameraController::updateMouseWheelInputMMOMode);
            vui::InputDispatcher::mouse.onMotion += makeDelegate(this, &CameraController::updateMouseMotionInputMMOMode);
            vui::InputDispatcher::mouse.onButtonDown += makeDelegate(this, &CameraController::updateMouseButtonDownInputMMO);
            vui::InputDispatcher::mouse.onButtonUp += makeDelegate(this, &CameraController::updateMouseButtonUpInputMMO);
            break;
        case CameraMode::MOUSELOCK:
            vui::InputDispatcher::mouse.onWheel += makeDelegate(this, &CameraController::updateMouseWheelInput);
            break;
        case CameraMode::FREE_LOOK:
            vui::InputDispatcher::mouse.onMotion += makeDelegate(this, &CameraController::updateMouseMotionInputFreeLookMode);
            break;
        case CameraMode::FIRST_PERSON:
            vui::InputDispatcher::mouse.onMotion += makeDelegate(this, &CameraController::updateMouseMotionInputFirstPersonMode);
            mIsMouseHidden = true;
            mLastMousePositionBeforeRelative = i32v2(mWindow.getWidth() * 0.5f, mWindow.getHeight() * 0.5f);
            break;
        case CameraMode::NONE:
        default:
            break;
    }
    static_assert(e_cast(CameraMode::COUNT) == 6, "Update any input register");

}

void CameraController::updateCameraCartesianMode(f32 frameAlpha) {

    // Must have a follow
    if (mEntityFollow == entt::null) {
        return;
    }

    // TODO: Delta time dependent?

    f32v3 followTargetPos = getFollowTargetPos(frameAlpha);

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

void CameraController::updateCameraMMOMode(f32 frameAlpha)
{
    // Must have a follow
    if (mEntityFollow == entt::null) {
        return;
    }

    mCameraBoomLengthTweener.update(1.0f); // TODO: Use deltatime

    // TODO: Delta time dependent?

    f32v3 followTargetPos = getFollowTargetPos(frameAlpha);
    followTargetPos.z += sDebugOptions.mCameraZHeight + SQ(mCameraBoomLengthTweener.getCurr() * 0.5f);

    const f32v3 lookAtOffset = mCamera.getDirection() * sDebugOptions.mCameraXYDistance * 2.0f * mCameraBoomLengthTweener.getCurr();
    mCamera.setPosition(followTargetPos - lookAtOffset);
    mCamera.lookAt(followTargetPos);

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

void CameraController::updateCameraFirstPersonMode(f32 frameAlpha) {
    const f32v3 followTargetPos = getFollowTargetPos(frameAlpha);
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

void CameraController::updateMouseWheelInput(Sender s, const vui::MouseWheelEvent& evnt) {
    mCameraPositionTweener.mTarget.z = glm::clamp(mCameraPositionTweener.mTarget.z + evnt.dy * mCameraPositionTweener.mTarget.z * -0.2f, CAMERA_ZOOM_RANGE.x, CAMERA_ZOOM_RANGE.y);
}

void CameraController::updateMouseWheelInputMMOMode(Sender s, const vui::MouseWheelEvent& evnt) {
    mCameraBoomLengthTweener.mTarget = glm::clamp(mCameraBoomLengthTweener.mTarget + (f32)evnt.dy * mCameraBoomLengthTweener.mTarget * -0.2f, 0.5f, 20.0f /*3.0f*/);
}

void CameraController::updateMouseMotionInputFreeLookMode(Sender s, const vui::MouseMotionEvent& evnt) {
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::RIGHT)) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateMouseMotionInputMMOMode(Sender s, const vui::MouseMotionEvent& evnt) {
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::RIGHT)) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateMouseMotionInputFirstPersonMode(Sender s, const vui::MouseMotionEvent& evnt) {
    if (mIsMouseHidden) {
        constexpr f32 ROTATE_SPEED = 0.002f;
        mCamera.applyRotation(evnt.dy * ROTATE_SPEED, evnt.dx * ROTATE_SPEED);
    }
}

void CameraController::updateKeyInputCartesianMode(Sender sender, const vui::KeyEvent& evnt) {
    if (evnt.keyCode == VKEY_Q) {
        mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][1];
        mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
    }
    else if (evnt.keyCode == VKEY_E) {
        mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][0];
        mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
    }
}

void CameraController::updateMouseButtonDownInputMMO(Sender s, const vui::MouseButtonEvent& evnt) {
    // We cant set SDL mouse mode inside the event handler or it causes bugs
    if (evnt.button == vorb::ui::MouseButton::RIGHT) {
        mIsMouseHidden = true;
        mLastMousePositionBeforeRelative = i32v2(evnt.x, evnt.y);
    }
}

void CameraController::updateMouseButtonUpInputMMO(Sender s, const vui::MouseButtonEvent& evnt) {
    if (evnt.button == vorb::ui::MouseButton::RIGHT) {
        mIsMouseHidden = false;
    }
}

f32v3 CameraController::getFollowTargetPos(f32 frameAlpha) {
    // If we have no follow target just return current position
    // TODO: Pretty sure this is wrong
    if (mEntityFollow == entt::null) {
        return mCamera.getPosition();
    }

    const EntityComponentSystem& ecs = mWorld.getECS();
    const PhysicsComponent& physCmp = ecs.mRegistry.get<PhysicsComponent>(mEntityFollow);
    const f32v2& targetXYPos = physCmp.getXYInterpolated(frameAlpha);
    const f32 targetZPos = physCmp.getZInterpolated(frameAlpha);

    return f32v3(targetXYPos.x, targetXYPos.y, targetZPos);
}
