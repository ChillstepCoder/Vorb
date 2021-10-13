#pragma once

struct ui32AABB2 {
    ui32AABB2() = default;
    ui32AABB2(ui32 v) : x(v), y(v), width(v), height(v) {};
    ui32AABB2(ui32 x, ui32 y, ui32 width, ui32 height) : x(x), y(y), width(width), height(height) {};

    ui32& operator[](int i) { return data[i]; }

    ui32v2 getCenter() const { return pos + dims / 2u; }

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

struct f32AABB3 {
    f32AABB3() = default;
    f32AABB3(f32 v) : x(v), y(v), z(v), width(v), depth(v), height(v) {};
    f32AABB3(f32 x, f32 y, f32 z, f32 width, f32 depth, f32 height) : x(x), y(y), z(z), width(width), depth(depth), height(height) {};

    f32& operator[](int i) { return data[i]; }

    f32v3 getCenter() const { return pos + dims / 2.0f; }
    f32 getMaxX() const { return x + width; }
    f32 getMaxY() const { return y + depth; }
    f32 getMaxZ() const { return z + height; }
    f32 getMax(ui32 d) const { return pos[d] + dims[d]; }

    union {
        f32 data[6];
        struct {
            union {
                struct {
                    f32 x;
                    f32 y;
                    f32 z;
                };
                f32v3 pos;
            };
            union {
                struct {
                    f32 width;
                    f32 depth;
                    f32 height;
                };
                f32v3 dims;
            };
        };
    };
};


// c = center, r = halfwidth
inline bool testAABBAABB_SIMD(const ui32AABB2& a, const ui32AABB2& b) {
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
// TODO: this is confusing, inclusive for stockpile, noninclusive for AABB splits for cities
inline bool pointIsWithinAABBInclusive(const ui32v2& point, const ui32AABB2& aabb) {
    return point.x >= aabb.x &&
        point.y >= aabb.y &&
        point.x < aabb.x + aabb.width &&
        point.y < aabb.y + aabb.height;
}

inline bool pointIsWithinAABB(const ui32v2& point, const ui32AABB2& aabb) {
    return point.x > aabb.x &&
        point.y > aabb.y &&
        point.x < aabb.x + aabb.width &&
        point.y < aabb.y + aabb.height;
}