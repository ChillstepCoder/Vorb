#pragma once

struct DynamicModelInstanceState {
    glm::quat orientation;
    f32v3 position;
    ModelID modelId;
};
