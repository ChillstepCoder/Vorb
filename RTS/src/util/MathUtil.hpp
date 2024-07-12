#pragma once

#define DEG_TO_RAD(x) ((x) * M_PIf / 180.0f)
#define RAD_TO_DEG(x) ((x) * 180.0f / M_PIf)
#define SQ(x) ((x) * (x))

constexpr f32 MATH_EPSILON = 0.00001f;

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
    inline f32v3 rotateVectorYawRad(const f32v3& in, float angleRad) {
        const float cs = cosf(angleRad);
        const float sn = sinf(angleRad);

        f32v3 rv;
        rv.x = in.x * cs - in.y * sn;
        rv.y = in.x * sn + in.y * cs;
        rv.z = in.z;
        return rv;
    }
    inline f32v3 rotateVectorYaw(const f32v3& in, float angleDeg) {
        return rotateVectorYawRad(in, DEG_TO_RAD(angleDeg));
    }
    inline f32v2 rotateVector2DRad(const f32v2& in, float angleRad) {
        const float cs = cosf(angleRad);
        const float sn = sinf(angleRad);

        f32v2 rv;
        rv.x = in.x * cs - in.y * sn;
        rv.y = in.x * sn + in.y * cs;
        return rv;
    }
    // Angle 0 is +X, 90 is +Y, 180 is -X, 270 is -Y
    inline f32v2 getNormalVectorFromAngleRad(float angleRad) {
        return f32v2(cosf(angleRad), sinf(angleRad));
    }
    inline f32v2 rotateVector2D(const f32v2& in, float angleDeg) {
        return rotateVector2DRad(in, DEG_TO_RAD(angleDeg));
    }
    inline f32v3 computeInitialProjectileVelocityToTarget(f32v3 projectileStart, f32v3 target, f32 arrivalTime, f32 gravity) {
        f32v3 velocity;
        velocity.x = (target.x - projectileStart.x) / arrivalTime;
        velocity.y = (target.y - projectileStart.y) / arrivalTime;
        // z = z0 + v0*t - 0.5*g*t^2
        velocity.z = (target.z - projectileStart.z - 0.5 * gravity * SQ(arrivalTime)) / arrivalTime;
        return velocity;
    }

    inline std::pair<float /*distSq*/, float /*time*/> computePointToLineSegmentDistanceSQAndT(f32v2 p, f32v2 p1, f32v2 p2) {
        f32v2 diff = p2 - p1;

        // Check if the line segment is a point
        if (diff.x == 0 && diff.y == 0)
            return std::make_pair(glm::length(p - p1), 0.0f);

        // Calculate the t that minimizes the distance
        float t = glm::dot(p - p1, diff) / glm::dot(diff, diff);

        // If the t is outside the segment use the endpoint
        if (t < 0.f) {
            t = 0.f;
            diff = p - p1;
        }
        else if (t > 1.f) {
            t = 1.f;
            diff = p - p2;
        }
        else {
            // Project to the point to the line to get the minimum distance
            f32v2 projection = p1 + t * diff;
            diff = p - projection;
        }

        return std::make_pair(glm::length2(diff), t);
    }
    inline float computePointToLineSegmentDistanceSQ(f32v2 p, f32v2 p1, f32v2 p2) {
        return computePointToLineSegmentDistanceSQAndT(p, p1, p2).first;
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
    template <typename T>
    inline T lerpWithDeltaTime(T a, T b, float lerpAlpha, float elapsedSec) {
        const float fraction = 1.0f - pow(1.0f - lerpAlpha, elapsedSec);
        return lerp(a, b, fraction);
    }

    template <typename T, typename A>
    inline void accelerateWithDeltaTime(T& inOutPosition, T& inOutVelocity, A accelerationForce, f32 elapsedSec) {
        inOutPosition += inOutVelocity * elapsedSec + 0.5f * accelerationForce * SQ(elapsedSec);
        inOutVelocity += accelerationForce * elapsedSec;
    }

    // Returns deltaPos,deltaVel
    template <typename T, typename A>
    inline std::pair<T /*deltaPos*/, A/*deltaVel*/> accelerateWithDeltaTime(T inVelocity, A accelerationForce, f32 elapsedSec) {
        const T deltaPosition = inVelocity * elapsedSec + 0.5f * accelerationForce * SQ(elapsedSec);
        const A deltaVelocity = accelerationForce * elapsedSec;
        return { deltaPosition, deltaVelocity };
    }

    inline f32 dragForceWithDeltaTime(f32 dragForce, f32 elapsedSec) {
        return pow(1.0f - dragForce, elapsedSec);
    }

    // Normalizes the angle to be between -PI and PI
    inline float normalizeAngle(float angle) {
        while (angle > M_PIf) angle -= 2 * M_PIf;
        while (angle < -M_PIf) angle += 2 * M_PIf;
        return angle;
    }
    // Multiply rotationSpeed by deltatime
    inline float rotateYawToTarget(float currentYaw, float targetYaw, float rotationSpeed) {
        currentYaw = normalizeAngle(currentYaw);
        targetYaw = normalizeAngle(targetYaw);

        float deltaYaw = targetYaw - currentYaw;

        // Select the direction of rotation to take the shortest path.
        deltaYaw = normalizeAngle(deltaYaw);

        // Apply the rotation speed.
        deltaYaw = std::clamp(deltaYaw, -rotationSpeed, rotationSpeed);

        // Compute the new yaw value.
        float newYaw = currentYaw + deltaYaw;

        return normalizeAngle(newYaw);
    }
    // Returns new angular speed
    inline float getNewAngularSpeedToTargetYawSmooth(f32 currentAngularSpeed, f32 currentYaw, f32 targetYaw, f32 maxAngularSpeed, f32 angularAcceleration, f32 elapsedSec) {
        float yawDifference = currentYaw - targetYaw;
        yawDifference = MathUtil::normalizeAngle(yawDifference);

        f32 desiredAngularSpeed = (fabs(yawDifference) < (maxAngularSpeed * maxAngularSpeed / (2.0f * angularAcceleration))) ?
            sqrt(fabs(yawDifference) * 2.0f * angularAcceleration) : maxAngularSpeed;
        // Pick sign
        desiredAngularSpeed *= (yawDifference < 0) - (yawDifference > 0);
        if (currentAngularSpeed < desiredAngularSpeed) {
            currentAngularSpeed += angularAcceleration * elapsedSec;
            if (currentAngularSpeed > desiredAngularSpeed) {
                currentAngularSpeed = desiredAngularSpeed;
            }
        }
        else if (currentAngularSpeed > desiredAngularSpeed) {
            currentAngularSpeed -= angularAcceleration * elapsedSec;
            if (currentAngularSpeed < desiredAngularSpeed) {
                currentAngularSpeed = desiredAngularSpeed;
            }
        }
        return currentAngularSpeed;
    }
    inline f32v2 directionFromYaw2D(f32 yaw) {
        return f32v2(cos(yaw), sin(yaw));
    }

    // +X is forward
    inline f32v3 directionFromYaw3D(f32 yaw) {
        return f32v3(cos(yaw), sin(yaw), 0.0f);
    }

    inline f32 yawFromDirection(const f32v2 dir) {
        return atan2(dir.y, dir.x);
    }

    inline f32m4 createTransformMatrix(const f32v3& translation, const glm::quat& orientation, f32 scale) {
        f32m4 matrix;
        
        // glm::mat4_cast expanded here so we can construct entire thing in most efficient way
        const f32 qxx(orientation.x * orientation.x);
        const f32 qyy(orientation.y * orientation.y);
        const f32 qzz(orientation.z * orientation.z);
        const f32 qxz(orientation.x * orientation.z);
        const f32 qxy(orientation.x * orientation.y);
        const f32 qyz(orientation.y * orientation.z);
        const f32 qwx(orientation.w * orientation.x);
        const f32 qwy(orientation.w * orientation.y);
        const f32 qwz(orientation.w * orientation.z);

        matrix[0][0] = scale * (1 - 2 * (qyy + qzz));
        matrix[0][1] = scale * (2 * (qxy + qwz));
        matrix[0][2] = scale * (2 * (qxz - qwy));
        matrix[0][3] = 0.0f;

        matrix[1][0] = scale * (2 * (qxy - qwz));
        matrix[1][1] = scale * (1 - 2 * (qxx + qzz));
        matrix[1][2] = scale * (2 * (qyz + qwx));
        matrix[1][3] = 0.0f;

        matrix[2][0] = scale * (2 * (qxz + qwy));
        matrix[2][1] = scale * (2 * (qyz - qwx));
        matrix[2][2] = scale * (1 - 2 * (qxx + qyy));
        matrix[2][3] = 0.0f;

        matrix[3][0] = translation.x;
        matrix[3][1] = translation.y;
        matrix[3][2] = translation.z;
        matrix[3][3] = 1.0f;

        return matrix;
    }
    inline f32 crossProduct2d(f32v2 v1, f32v2 v2) {
        return (v1.x * v2.y) - (v1.y * v2.x);
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