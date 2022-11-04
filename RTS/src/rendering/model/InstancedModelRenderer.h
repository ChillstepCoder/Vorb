#pragma once

struct InstanceData {
    f32v3 position;
    f32 rotation;
};

class InstancedModelRenderer
{
public:
    void renderModels();

    void addModel(ModelID modelID, f32v3 position, f32 rotation);
private:

};

