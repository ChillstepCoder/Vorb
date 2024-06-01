#pragma once
class DynamicModelComponent {
public:
    DynamicModelComponent(ModelID modelId) :
        modelId(modelId)
    {}

    ModelID modelId;
    // TODO: Variant? And perhaps ModelID can be 16 or 24 bit? Do we really expect more than 65536 models?
};

