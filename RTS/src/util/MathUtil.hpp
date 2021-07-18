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
            return x < 0.5 ? 4.0f * x * x * x : 1 - pow(-2 * x + 2, 3) / 2;
        }

        inline float easeInOutQuad(float x) {
            return x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2;
        }

        inline float easeInOutSine(float x) {
            return -(cos(M_PIF * x) - 1) / 2;
        }
    }
}