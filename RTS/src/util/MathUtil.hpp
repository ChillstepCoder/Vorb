#pragma once

#define DEG_TO_RAD(x) ((x) * M_PIf / 180.0f)
#define RAD_TO_DEG(x) ((x) * 180.0f / M_PIf)

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
}