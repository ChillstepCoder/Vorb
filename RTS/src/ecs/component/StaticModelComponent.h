#pragma once

class StaticModelComponent {
public:
    StaticModelComponent(ModelID modelId, f32 scale) :
        modelId(modelId), scale(scale) {
    }

    VORB_NON_COPYABLE_BUT_MOVABLE(StaticModelComponent);

    ModelID modelId;
    StaticModelInstanceID staticModelInstanceId = INVALID_STATIC_MODEL_INSTANCE_ID;
    f32 scale;
    // TODO: Variants?
};