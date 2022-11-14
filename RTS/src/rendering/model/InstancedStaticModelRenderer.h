#pragma once

#include "rendering/model/StaticModelInstance.h"

class Camera3D;
class InstancedStaticModelGatherer;
class Material;

struct StaticModelInstanceData {
    std::vector<StaticModelInstance> mInstances;
    VGBuffer mInstanceVbo = 0;
    bool mDirty = false;
};

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void renderModels(const Camera3D& camera);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    ui32 getNumModels() const;
private:
    std::map<ModelID, StaticModelInstanceData> mInstances;
    std::vector<StaticModelInstance> mInstancesToRender;

    const Material* mStandardMaterial = nullptr;
};

