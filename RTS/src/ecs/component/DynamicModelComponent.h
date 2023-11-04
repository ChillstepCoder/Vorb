#pragma once
class DynamicModelComponent {
public:
    DynamicModelComponent(ModelID modelId) :
        modelId(modelId)
    {}

    ModelID modelId;
};

