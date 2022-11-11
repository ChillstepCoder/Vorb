#pragma once

class Camera3D;

struct StaticModelInstance {
    f32v3 pos;
    f32 rotation;
};

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void renderModels(const Camera3D& camera);

private:
    std::map<ModelID, std::vector<StaticModelInstance>> mInstances;
};

