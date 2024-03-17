#pragma once

template<typename T, int Dimensions>
struct AABB;

template<typename T>
using VVec2 = glm::vec<2, T, glm::defaultp>; // Assuming these are defined elsewhere.
template<typename T>
using VVec3 = glm::vec<3, T, glm::defaultp>;
template<typename T>
using VVec4 = glm::vec<4, T, glm::defaultp>;

template<typename T>
struct AABB<T, 2> {
    AABB() = default;
    AABB(T v) : x(v), y(v), width(v), depth(v) {}
    AABB(T x, T y, T width, T depth) : x(x), y(y), width(width), depth(depth) {}
    template<typename U>
    AABB(const AABB<U, 3>& other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), width(static_cast<T>(other.width)), depth(static_cast<T>(other.depth)) {}

    T& operator[](int i) { return data[i]; }
    bool operator==(const AABB& other) const { return x == other.x && y == other.y && width == other.width && depth == other.depth; }

    VVec2<T> getBottomLeft() const { return pos; }
    VVec2<T> getCenter() const { return pos + dims / T(2); }
    VVec2<T> getTopLeft() const { return pos + VVec2<T>(0, dims.y); }
    VVec2<T> getTopRight() const { return pos + dims; }
    VVec2<T> getBottomRight() const { return pos + VVec2<T>(dims.x, 0); }
    void getCorners(VVec2<T> aabbCorners[4]) const {
        aabbCorners[0] = { x, y };
        aabbCorners[1] = { x + width, y };
        aabbCorners[2] = { x, y + depth };
        aabbCorners[3] = { x + width, y + depth };
    }
    inline bool pointIsWithin(VVec2<T> point) {
        return point.x >= x && point.y >= y && point.x < x + width && point.y < y + depth;
    }

    union {
        VVec4<T> data;
        struct {
            union {
                struct {
                    T x, y;
                };
                VVec2<T> pos;
            };
            union {
                struct {
                    T width, depth;
                };
                VVec2<T> dims;
            };
        };
    };
};

template<typename T>
struct AABB<T, 3> {
    AABB() = default;
    AABB(T v) : x(v), y(v), z(v), width(v), depth(v), height(v) {}
    AABB(T x, T y, T z, T width, T depth, T height) : x(x), y(y), z(z), width(width), depth(depth), height(height) {}
    AABB(VVec3<T> pos, VVec3<T> dims) : pos(pos), dims(dims) {}

    T& operator[](int i) { return data[i]; }

    VVec3<T> getCenter() const { return pos + dims / T(2); }
    T getMaxX() const { return x + width; }
    T getMaxY() const { return y + depth; }
    T getMaxZ() const { return z + height; }
    T getMax(T d) const { return pos[d] + dims[d]; }

    union {
        T data[6];
        struct {
            union {
                struct {
                    T x, y, z;
                };
                VVec3<T> pos;
            };
            union {
                struct {
                    T width, depth, height;
                };
                VVec3<T> dims;
            };
        };
    };
};
using i16AABB2 = AABB<i16, 2>;
using i16AABB3 = AABB<i16, 3>;
using i32AABB2 = AABB<i32, 2>;
using i32AABB3 = AABB<i32, 3>;
using f32AABB2 = AABB<f32, 2>;
using f32AABB3 = AABB<f32, 3>;

struct BoundingSphere {
    f32v3 center = f32v3(0.0f);
    f32 radius = 0.0f;
};

inline BoundingSphere boundingSphereFromAABB(const f32AABB3& aabb) {
    BoundingSphere rv;
    rv.center = aabb.getCenter();
    rv.radius = sqrt(SQ(aabb.dims.x * 0.5f) + SQ(aabb.dims.y * 0.5f) + SQ(aabb.dims.z * 0.5f));
    return rv;
}

// c = center, r = halfwidth
inline bool testAABBAABB_SIMD(const i32AABB2& a, const i32AABB2& b) {
    // SIMD optimized AABB-AABB test
    // Optimized by removing conditional branches
    const i64 cxa = (i64)(a.x + a.width / 2);
    const i64 cya = (i64)(a.y + a.depth / 2);
    const i64 cxb = (i64)(b.x + b.width / 2);
    const i64 cyb = (i64)(b.y + b.depth / 2);
    // -1 to check if within
    const bool x = std::fabs((i64)cxa - (i64)cxb) <= (((i64)a.width + (i64)b.width) / 2) - 1;
    const bool y = std::fabs((i64)cya - (i64)cyb) <= (((i64)a.depth + (i64)b.depth) / 2) - 1;

    return x && y;
}

// TODO: Shared
// TODO: this is confusing, inclusive for stockpile, noninclusive for AABB splits for cities
inline bool pointIsWithinAABBInclusive(const i32v2& point, const i32AABB2& aabb) {
    return point.x >= aabb.x &&
        point.y >= aabb.y &&
        point.x < aabb.x + aabb.width &&
        point.y < aabb.y + aabb.depth;
}

inline bool pointIsWithinAABB(const i32v2& point, const i32AABB2& aabb) {
    return point.x > aabb.x &&
        point.y > aabb.y &&
        point.x < aabb.x + aabb.width &&
        point.y < aabb.y + aabb.depth;
}