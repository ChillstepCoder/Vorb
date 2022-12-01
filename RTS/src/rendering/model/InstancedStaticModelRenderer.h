#pragma once

#include "rendering/model/StaticModelInstance.h"

class Camera3D;
class InstancedStaticModelGatherer;
class Material;
class GLIndirectBuffer;

struct StaticModelInstanceData {
    StaticModelInstanceData() = default;
    ~StaticModelInstanceData();

    std::vector<StaticModelInstance> mInstances;
    std::unique_ptr<GLIndirectBuffer> mDrawCommands;
    VGBuffer mTransformsVbo = 0;
    ui32 mTransformsVboSizeBytes = 0;
    bool mDirtyDrawCommands = false;
};

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void renderModels(const Camera3D& camera);
    void renderModelShadows(const Camera3D& camera, const f32* shadowDistances);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    ui32 getNumModels() const;
private:
    std::map<ModelID, StaticModelInstanceData> mInstances;

    const Material* mStandardMaterial = nullptr;
    const Material* mShadowMapperMaterial = nullptr;
};

