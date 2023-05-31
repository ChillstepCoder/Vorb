#pragma once

#define DEG_TO_RAD(x) ((x) * M_PIf / 180.0f)
#define RAD_TO_DEG(x) ((x) * 180.0f / M_PIf)
#define SQ(x) ((x) * (x))

constexpr f32 MATH_EPSILON = 0.00001f;

#include "LinearMath/btVector3.h"

inline int intFloor(double x) {
    int i = (int)x;
    return i - (i > x);
}
inline int intFloor(float x) {
    int i = (int)x;
    return i - (i > x);
}

// Increments i in modulo 3 for triple buffering
inline void incrementMod3(OUT int& i) {
    i = (i + 1) % 3;
}

namespace {
    inline f32v3 btVector3ToF32v3(const btVector3& v) {
        return f32v3(v.x(), v.y(), v.z());
    }
    inline btVector3 f32v3ToBtVector3(const f32v3& v) {
        return btVector3(v.x, v.y, v.z);
    }
    inline const i8v3 compressNormal(const f32v3& normal) {
        return {
            (i8)glm::clamp(normal.x * 127.0f, -127.0f, 127.0f),
            (i8)glm::clamp(normal.y * 127.0f, -127.0f, 127.0f),
            (i8)glm::clamp(normal.z * 127.0f, -127.0f, 127.0f)
        };
    }
}

namespace MathUtil {
    inline f32v2 RotateVector(float x, float y, float angleDeg) {
        const float angleRad = DEG_TO_RAD(angleDeg);
        const float cs = cosf(angleRad);
        const float sn = sinf(angleRad);

        f32v2 rv;
        rv.x = x * cs - y * sn;
        rv.y = x * sn + y * cs;
        return rv;
    }
    inline f32v3 rotateVectorYaw(const f32v3& in, float angleDeg) {
        const float angleRad = DEG_TO_RAD(angleDeg);
        const float cs = cosf(angleRad);
        const float sn = sinf(angleRad);

        f32v3 rv;
        rv.x = in.x * cs - in.y * sn;
        rv.y = in.x * sn + in.y * cs;
        rv.z = in.z;
        return rv;
    }
    inline f32v2 rotateVector2D(const f32v2& in, float angleDeg) {
        const float angleRad = DEG_TO_RAD(angleDeg);
        const float cs = cosf(angleRad);
        const float sn = sinf(angleRad);

        f32v2 rv;
        rv.x = in.x * cs - in.y * sn;
        rv.y = in.x * sn + in.y * cs;
        return rv;
    }

    namespace Easing {
        inline float easeInOutCubic(float x) {
            return x < 0.5f ? 4.0f * x * x * x : 1 - powf(-2 * x + 2, 3) / 2;
        }

        inline float easeInOutQuad(float x) {
            return x < 0.5f ? 2 * x * x : 1 - powf(-2 * x + 2, 2) / 2;
        }

        inline float easeInOutSine(float x) {
            return -(cos(M_PIF * x) - 1) / 2;
        }
    }

    inline bool areParallelSegmentsTouching(const ui32v2& pa1, const ui32v2& pa2, const ui32v2& pb1, const ui32v2& pb2) {
        if (pa1.x == pa2.x) {
            assert(pb1.x == pb2.x);
            // Vertical segments
            if (pa1.x == pb1.x) {
                if (pb2.y < pa1.y || pb1.y > pa2.y) {
                    return false;
                }
                return true;
            }
        }
        else {
            assert(pa1.y == pa2.y);
            assert(pb1.y == pb2.y);
            // Horizontal segments
            if (pa1.y == pb1.y) {
                if (pb2.x < pa1.x || pb1.x > pa2.x) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    template <unsigned int p>
    int constexpr intpow(const int x)
    {
        if constexpr (p == 0) return 1;
        if constexpr (p == 1) return x;

        int tmp = intpow<p / 2>(x);
        if constexpr ((p % 2) == 0) { return tmp * tmp; }
        else { return x * tmp * tmp; }
    }

    namespace Detail
    {
        double constexpr sqrtNewtonRaphson(double x, double curr, double prev)
        {
            return curr == prev
                ? curr
                : sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
        }
    }

    /*
    * Constexpr version of the square root
    * Return value:
    *   - For a finite and non-negative value of "x", returns an approximation for the square root of "x"
    *   - Otherwise, returns NaN
    */
    double constexpr sqrtd(double x)
    {
        return x >= 0 && x < std::numeric_limits<double>::infinity()
            ? Detail::sqrtNewtonRaphson(x, x, 0)
            : std::numeric_limits<double>::quiet_NaN();
    }
}

#define DECL_VEC2_LESS(T) \
template<> \
struct std::less<T> \
{ \
    bool operator() (const T& a, const T& b) const \
    { \
        if (a.x < b.x) return true; \
        if (a.x > b.x) return false; \
        return a.y < b.y; \
    } \
};

// Template specialization for storing these values as keys in a set/map
namespace std {
    DECL_VEC2_LESS(ui32v2);
    DECL_VEC2_LESS(i32v2);
    DECL_VEC2_LESS(f32v2);
}