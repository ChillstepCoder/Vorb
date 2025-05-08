#pragma once

constexpr int CORNER_COUNT = 4;
enum class CornerWinding {
    BOTTOM_LEFT = 0,
    BOTTOM_RIGHT = 1,
    TOP_LEFT = 2,
    TOP_RIGHT = 3,
    NONE
};
const ui32v2 CORNER_WINDING_OFFSETS[CORNER_COUNT] = {
    ui32v2(0,  0), // BOTTOM_LEFT
    ui32v2(1,  0), // BOTTOM_RIGHT
    ui32v2(0,  1), // TOP_LEFT
    ui32v2(1,  1), // TOP_RIGHT
};