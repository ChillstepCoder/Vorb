#pragma once

struct YawPitchComponent {
    union {
        struct {
            f32 mYaw;
            f32 mPitch;
        };
        f32v2 mYawPitch = f32v2(0.0f);
    };
};