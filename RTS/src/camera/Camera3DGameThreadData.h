#pragma once

// Data the game thread wants from the render context
struct Camera3DGameThreadData {
    f32 yaw;
    f32v3 worldPos;
    f32v3 direction;
};
