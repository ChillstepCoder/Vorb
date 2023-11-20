#pragma once

constexpr f32 HEIGHT_STEP = 0.1f;
constexpr f32 MAX_HEIGHT = HEIGHT_STEP * 32767.0f;
constexpr f32 MIN_HEIGHT = -MAX_HEIGHT;

typedef i16 CompressedHeight;

inline f32 uncompressHeight(CompressedHeight height) {
    return height * HEIGHT_STEP;
}
inline CompressedHeight compressHeight(f32 height) {
    return CompressedHeight(glm::round(height / HEIGHT_STEP));
}