#pragma once

struct DynamicModelRenderState {
    glm::quat orientation;
    f32v3 position;
    ModelID modelId;
};
