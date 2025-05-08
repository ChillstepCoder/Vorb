#pragma once

struct WorldTextRenderState {
    std::string text; // TODO: Arena allocator? Or alternate string class?
    color4 color;
    f32v3 worldPos;
};