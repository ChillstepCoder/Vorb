#pragma once

class Camera3D;
class TerrainMeshManager;
class GrassMeshManager;
class IWorld;
class RenderState;

class CliWorldInterface
{
public:
    CliWorldInterface();
    virtual ~CliWorldInterface();

    void tickClient(IWorld& world);
    // Pure virtual interface
    virtual void onFrameBegin() = 0;
    virtual void frameUpdate(const Camera3D& camera, f32 elapsedSec) = 0;

protected:
    void initClient(IWorld& world);

    void onWorldBeginClient(IWorld& world);

    void updateRenderState(IWorld& world);
    void updateEntitiesRenderState(IWorld& world, RenderState& renderState);
    void updateDebugRenderState(IWorld& world, RenderState& renderState);

    // Sunlight
    float mSunHeight = 1.0f;
    f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
    float mTimeOfDay = 0.0f; // span of 24:00
    f32v3 mSunColor = f32v3(1.0f);
    f32m4 mSkyRotMatrix = f32m4(1.0f);

private:
    IWorld* mCliWorld = nullptr;
};

