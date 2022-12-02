#include "stdafx.h"
#include "InstancedStaticModelGatherer.h"

void InstancedStaticModelGatherer::addInstance(ModelID modelId, const f32v3& position, f32 rotation) {
    glm::mat4& matrix = mInstances[modelId].emplace_back().matrix;
    matrix = glm::rotate(glm::translate(glm::mat4(1.0f), position), rotation, f32v3(0.0f, 0.0f, 1.0f));
}

void InstancedStaticModelGatherer::addInstance(ModelID modelId, const f32v3& position, const f32v3& normal, f32 rotation) {
    glm::mat4& matrix = mInstances[modelId].emplace_back().matrix;

    glm::vec3 rotationZ = normal;
    glm::vec3 rotationX = glm::normalize(glm::cross(glm::vec3(0, -1, 0), rotationZ));
    glm::vec3 rotationY = glm::normalize(glm::cross(rotationZ, rotationX));
    // TODO: Optimize by building a mat3, doing all the rotate translate, then manually set up mat4
    glm::mat4 normRotation(rotationX.x, rotationY.x, rotationZ.x, 0.0f,
                       rotationX.y, rotationY.y, rotationZ.y, 0.0f,
                       rotationX.z, rotationY.z, rotationZ.z, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f );

    matrix = glm::rotate(glm::translate(glm::mat4(1.0f), position) * normRotation, rotation, f32v3(0.0f, 0.0f, 1.0f));
}
