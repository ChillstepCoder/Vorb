#pragma once

class Camera3D;

// Cascading shadow maps
// https://learnopengl.com/Guest-Articles/2021/CSM
class ShadowRenderer
{
public:
    ShadowRenderer();

    void beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld);

private:
    f32m4 mLightV;
    f32m4 mLightP;
    f32m4 mLightVP;

    VGFramebuffer mLightFBO;
    VGTexture mLightDepthMaps;
};

