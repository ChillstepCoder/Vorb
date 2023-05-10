#pragma once

class Camera3D;
class TimeOfDayManager;

class UboHelpers
{
public:
    static void allocateGlobalUbo(VGBuffer& ubo);
    static void allocateCameraUbo(VGBuffer& ubo);
    static void uploadCameraUbo(VGBuffer ubo, const Camera3D& camera);
    static void uploadGlobalUbo(VGBuffer ubo, const Camera3D& camera, const f32v3& playerPos, const f32v3& sunPosition, const TimeOfDayManager& timeOfDayManager);
};

