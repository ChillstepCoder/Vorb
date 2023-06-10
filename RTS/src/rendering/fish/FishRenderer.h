#pragma once

class IWorld;
class Camera3D;
class GLIndirectBuffer;
class Mesh;
class MaterialShader;

struct FishInstanceTransform {
    f32v3 mPosition;
    f32 mYaw;
};

struct FishInstanceData {
    FishInstanceTransform* mMappedTransformBuffer;
    VGBuffer mInstanceTransformBuffer;
    const Mesh* mMesh = nullptr;
    // GLIndirectBuffer
};

class FishRenderer {
public:
    FishRenderer();
    ~FishRenderer();

    void renderFishEcosystem(const Camera3D& camera, const IWorld& world);
    void debugRenderFishEcosystem(const IWorld& world);

private:
    void addFishInstance(FishID fish, f32v3 pos, f32 yaw);
    int mDebugTickCounter = 0;

    // TODO: Can we guarentee all fish exist in one VBO?
    // One buffer per fish ID
    std::vector<FishInstanceData> mFishInstanceData;
    std::vector<ui32> mInstanceCountsThisFrame;
    GLsync mFence[3] = { 0 };
    int mFrameIndex = 0;
    const MaterialShader* mFishShader;
};

