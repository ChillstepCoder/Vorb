#pragma once

struct ui32AABB {
    ui32AABB() = default;
    ui32AABB(ui32 v) : x(v), y(v), width(v), height(v) {};
    ui32AABB(ui32 x, ui32 y, ui32 width, ui32 height) : x(x), y(y), width(width), height(height) {};

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


// c = center, r = halfwidth
inline bool testAABBAABB_SIMD(const ui32AABB& a, const ui32AABB& b) {
    // SIMD optimized AABB-AABB test
    // Optimized by removing conditional branches
    const i64 cxa = a.x + a.width / 2;
    const i64 cya = a.y + a.height / 2;
    const i64 cxb = b.x + b.width / 2;
    const i64 cyb = b.y + b.height / 2;
    // -1 to check if within
    const bool x = std::fabs((i64)cxa - (i64)cxb) <= (((i64)a.width + (i64)b.width) / 2) - 1;
    const bool y = std::fabs((i64)cya - (i64)cyb) <= (((i64)a.height + (i64)b.height) / 2) - 1;

    return x && y;
}

// TODO: Shared
inline bool pointIsWithinAABB(const ui32v2& point, const ui32AABB& aabb) {
    return point.x > aabb.x &&
        point.y > aabb.y &&
        point.x < aabb.x + aabb.width &&
        point.y < aabb.y + aabb.height;
}