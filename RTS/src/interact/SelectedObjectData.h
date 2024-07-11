#pragma once

struct SelectedObjectData {
    glm::quat orientation;
    f32v3 position;
    ModelID modelId = INVALID_MODEL_ID;
    f32 scale;
    f32 textZOffset;
    std::string text;
    color4 textColor;
};