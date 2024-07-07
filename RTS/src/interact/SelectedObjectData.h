#pragma once

struct SelectedObjectData {
    glm::quat orientation;
    f32v3 position;
    ModelID modelId = INVALID_MODEL_ID;
};