#pragma once

struct ui32AABB {

    ui32& operator[](int i) { return data[i]; }

    union {
        ui32v4 data;
        struct {
            union {
                struct {
                    ui32 x;
                    ui32 y;
                };
                ui32v2 pos;
            };
            union {
                struct {
                    ui32 width;
                    ui32 height;
                };
                ui32v2 dims;
            };
        };
    };
};