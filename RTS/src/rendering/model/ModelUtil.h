#pragma once

namespace ModelUtil {
    inline f32m4 computeTransformMatrixForModel(const f32v3& position, f32 yaw) {
        // TODO: We can construct the matrix manually to optimize out a lot of work
        return glm::rotate(glm::translate(glm::mat4(1.0f), position), yaw, f32v3(0.0f, 0.0f, 1.0f));
    }

    inline f32m4 computeTransformMatrixForModel(const f32v3& position, const f32v3& normal, f32 yaw) {
        const f32v3 rotationZ = normal;
        const f32v3 rotationX = glm::normalize(glm::cross(f32v3(0, -1, 0), rotationZ));
        const f32v3 rotationY = glm::normalize(glm::cross(rotationZ, rotationX));
        // TODO: Optimize by building a mat3, doing all the rotate translate, then manually set up mat4
        const f32m4 normRotation(
            rotationX.x, rotationY.x, rotationZ.x, 0.0f,
            rotationX.y, rotationY.y, rotationZ.y, 0.0f,
            rotationX.z, rotationY.z, rotationZ.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        // TODO: We can construct the matrix manually (maybe) to optimize out a lot of work
        return glm::rotate(glm::translate(glm::mat4(1.0f), position) * normRotation, yaw, f32v3(0.0f, 0.0f, 1.0f));
    }
}
