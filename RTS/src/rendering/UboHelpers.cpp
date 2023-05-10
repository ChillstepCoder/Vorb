#include "stdafx.h"
#include "UboHelpers.h"

#include "camera/Camera3D.h"

#include "time/TimeOfDayManager.h"
#include "rendering/GlobalUboData.h"

void UboHelpers::allocateGlobalUbo(VGBuffer& ubo) {
    glCreateBuffers(1, &ubo);
    glNamedBufferStorage(ubo, sizeof(GlobalUboData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_GLOBAL_UBO, ubo);
}

void UboHelpers::allocateCameraUbo(VGBuffer& ubo) {
    glCreateBuffers(1, &ubo);
    glNamedBufferStorage(ubo, CAMERA_MATRICES_BYTE_SIZE + sizeof(CameraUboData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_CAMERA_UBO, ubo);
}

void UboHelpers::uploadCameraUbo(VGBuffer ubo, const Camera3D& camera) {
    CameraUboData uboData;
    uboData.CameraPos = camera.getPosition();
    uboData.CameraFront = camera.getFrontVector();
    uboData.CameraRight = camera.getRightVector();
    uboData.CameraUp = camera.getUpVector();
    uboData.CameraZRange = f32v2(camera.getZNear(), camera.getZFar());
    // Camera matrices
    glNamedBufferSubData(ubo, 0, CAMERA_MATRICES_BYTE_SIZE, &camera.getViewMatrix()[0][0]);
    // Rest of the UBO
    glNamedBufferSubData(ubo, CAMERA_MATRICES_BYTE_SIZE, sizeof(CameraUboData), &uboData);
}

void UboHelpers::uploadGlobalUbo(VGBuffer ubo, const Camera3D& camera, const f32v3& playerPos, const f32v3& sunPosition, const TimeOfDayManager& timeOfDayManager) {
    // Global UBO
    GlobalUboData uboData;
    uboData.Time = sTotalTimeSeconds;
    uboData.TimeOfDay = timeOfDayManager.getTimeOfDayHours();
    uboData.PlayerPosWorld = playerPos;
    uboData.SunColor = timeOfDayManager.getSunColor();
    uboData.SunHeight = timeOfDayManager.getSunHeight();
    uboData.SunPosition = sunPosition;
    uboData.SunPositionCameraRelative = glm::normalize(f32v3(camera.getViewMatrix() * f32v4(sunPosition.x, sunPosition.y, sunPosition.z, 1.0f)));
    uboData.SunRight = glm::normalize(glm::cross(sunPosition, f32v3(0.0f, 0.0f, 1.0f)));
    uboData.SunUp = glm::normalize(glm::cross(sunPosition, uboData.SunRight));
    // Upload data
    glNamedBufferSubData(ubo, 0, sizeof(GlobalUboData), &uboData);
}
