#pragma once

class IWorld;
class Camera3D;

class FishRenderer
{
public:
    void renderFishEcosystem(const Camera3D& camera, const IWorld& world);
    void debugRenderFishEcosystem(const IWorld& world);

private:
    int mDebugTickCounter = 0;
};

