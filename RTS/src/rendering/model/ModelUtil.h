#pragma once

namespace ModelUtil {
    inline f32m4 computeTransformMatrixForModel(f32v3 position, f32 yaw) {
        const float s = glm::sin(yaw);
        const float c = glm::cos(yaw);

        return f32m4(
            c, -s, 0.0f, 0.0f,
            s, c, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            position.x, position.y, position.z, 1.0f
        );
    }

    inline f32m4 computeTransformMatrixForModel(f32v3 position, f32v3 normal, f32 yaw) {

        // Calculate rotationX and rotationY
        const f32v3 rotationX = glm::normalize(glm::cross(f32v3(0, 1, 0), normal));
        const f32v3 rotationY = glm::cross(normal, rotationX); // Already normalized

        // Precalculate sin and cos of yaw
        const float s = std::sin(yaw);
        const float c = std::cos(yaw);

        // Construct the rotation matrix (normal-based rotation followed by yaw) for glm counterclockwise rotation
        return f32m4(
            rotationX.x * c + rotationX.y * s, rotationY.x * c + rotationY.y * s, normal.x * c + normal.y * s, 0.0f,
            rotationX.x * -s + rotationX.y * c, rotationY.x * -s + rotationY.y * c, normal.x * -s + normal.y * c, 0.0f,
            rotationX.z, rotationY.z, normal.z, 0.0f,
            position.x, position.y, position.z, 1.0f
        );
    }

    inline f32m4 computeTransformMatrixForModel(f32v3 position, f32 yaw, f32 scale) {
        const float s = glm::sin(yaw);
        const float c = glm::cos(yaw);
        return f32m4(
            c * scale, s * scale, 0.0f, 0.0f,
            -s * scale, c * scale, 0.0f, 0.0f,
            0.0f, 0.0f, scale, 0.0f,
            position.x, position.y, position.z, 1.0f
        );
    }

    inline f32m4 computeTransformMatrixForModel(f32v3 position, f32v3 normal, f32 yaw, f32 scale) {
        // Calculate rotationX and rotationY
        const f32v3 rotationX = glm::normalize(glm::cross(f32v3(0, 1, 0), normal));
        const f32v3 rotationY = glm::cross(normal, rotationX); // Already normalized
        // Precalculate sin and cos of yaw
        const float s = std::sin(yaw);
        const float c = std::cos(yaw);
        // Construct the rotation matrix (normal-based rotation followed by yaw) for glm counterclockwise rotation
        return f32m4(
            (rotationX.x * c + rotationX.y * s) * scale, (rotationY.x * c + rotationY.y * s) * scale, (normal.x * c + normal.y * s) * scale, 0.0f,
            (rotationX.x * -s + rotationX.y * c) * scale, (rotationY.x * -s + rotationY.y * c) * scale, (normal.x * -s + normal.y * c) * scale, 0.0f,
            rotationX.z * scale, rotationY.z * scale, normal.z * scale, 0.0f,
            position.x, position.y, position.z, 1.0f
        );
    }
}
