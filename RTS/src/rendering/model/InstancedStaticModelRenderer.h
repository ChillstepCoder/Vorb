#pragma once

#include "rendering/model/StaticModelInstance.h"

class Camera3D;
class InstancedStaticModelGatherer;
class Material;

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void renderModels(const Camera3D& camera);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
private:
    std::map<ModelID, std::vector<StaticModelInstance>> mInstances;

    const Material* mStandardMaterial = nullptr;
};

