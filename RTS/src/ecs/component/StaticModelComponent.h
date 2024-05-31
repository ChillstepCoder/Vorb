#pragma once

class StaticModelComponent {
public:
    StaticModelComponent(ModelID modelId) :
        modelId(modelId) {
    }

    VORB_NON_COPYABLE_BUT_MOVABLE(StaticModelComponent);

    ModelID modelId;
    StaticModelInstanceID staticModelInstanceId = INVALID_STATIC_MODEL_INSTANCE_ID;
    // TODO: Variants?
};