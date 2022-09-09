#pragma once

struct cui32v2 {
    constexpr cui32v2() : x(0), y(0) {};
    constexpr cui32v2(ui32 v) : x(v), y(v) {};
    constexpr cui32v2(ui32 x, ui32 y) : x(x), y(y) {};
    operator ui32v2& () { return *reinterpret_cast<ui32v2*>(this); }
    operator const ui32v2& () const { return *reinterpret_cast<const ui32v2*>(this); }
    union {
        struct {
            ui32 x;
            ui32 y;
        };
        ui32v2 xy;
    };
};
static_assert(sizeof(cui32v2) == sizeof(ui32v2));

struct cf32v2 {
    constexpr cf32v2() : x(0.0f), y(0.0f) {};
    constexpr cf32v2(f32 v) : x(v), y(v) {};
    constexpr cf32v2(f32 x, f32 y) : x(x), y(y) {};
    operator f32v2& () { return *reinterpret_cast<f32v2*>(this); }
    operator const f32v2& () const { return *reinterpret_cast<const f32v2*>(this); }
    union {
        struct {
            f32 x;
            f32 y;
        };
        f32v2 xy;
    };
};
static_assert(sizeof(cf32v2) == sizeof(f32v2));
