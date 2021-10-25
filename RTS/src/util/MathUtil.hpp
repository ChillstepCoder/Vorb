#pragma once

#define DEG_TO_RAD(x) ((x) * M_PIf / 180.0f)
#define RAD_TO_DEG(x) ((x) * 180.0f / M_PIf)
#define SQ(x) ((x) * (x))

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
}