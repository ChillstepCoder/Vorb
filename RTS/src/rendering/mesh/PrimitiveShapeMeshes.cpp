#include "stdafx.h"
#include "PrimitiveShapeMeshes.h"

#include "Vorb/MeshGenerators.h"

#include "rendering/mesh/ModelMeshBuilder.h"
#include "rendering/mesh/MeshBuilderCommon.h"

Mesh& PrimitiveShapeMeshes::getOrGenerateShapeMesh(PrimitiveShapeType type) {
    assert(IS_RENDER_THREAD());

    // Generate lazily if needed
    if (mMeshes[e_cast(type)] == nullptr) {
        switch (type) {
            case PrimitiveShapeType::Sphere:
                generateSphereMesh();
                break;
            case PrimitiveShapeType::Plane:
                generatePlaneMesh();
                break;
            case PrimitiveShapeType::Cube:
                generateCubeMesh();
                break;
            case PrimitiveShapeType::Cylinder:
                generateCylinderMesh();
                break;
            default:
                assert(false);

        }
        static_assert(e_cast(PrimitiveShapeType::COUNT) == 4);
    }

    return *mMeshes[e_cast(type)];
}

void PrimitiveShapeMeshes::generateSphereMesh() {
    std::vector<ui32> indices;
    std::vector<f32v3> positions;
    vmesh::generateIcosphereMesh(3, indices, positions);
    std::vector<ui16> indices16(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices16[i] = indices[i];
    }

    std::vector<StaticModelVertex> vertices(positions.size(), StaticModelVertex{});
    for (size_t i = 0; i < vertices.size(); ++i) {
        StaticModelVertex& myVert = vertices[i];
        myVert.pos = positions[i];
        f32v3 normalFloat = glm::normalize(myVert.pos);
        f32v3 tangentFloat = glm::normalize(glm::cross(normalFloat, f32v3(0.0f, 0.0f, 1.0f)));
        if (normalFloat == f32v3(0.0f, 0.0f, 1.0f)) {
            tangentFloat = f32v3(1.0f, 0.0f, 0.0f);
        }
        else if (normalFloat == f32v3(0.0f, 0.0f, -1.0f)) {
            tangentFloat = f32v3(-1.0f, 0.0f, 0.0f);
        }
        // https://gamedev.stackexchange.com/questions/114412/how-to-get-uv-coordinates-for-sphere-cylindrical-projection
        f32v2 uvFloat;
        uvFloat.x = atan2(normalFloat.x, normalFloat.y) / (M_2_PI) + 0.5;
        uvFloat.y = normalFloat.z * 0.5 + 0.5;
        uvFloat = glm::clamp(uvFloat, f32v2(0.0), f32v2(1.0));
        myVert.uvsPacked.x = (ui16)(uvFloat.x * UINT16_MAX);
        myVert.uvsPacked.y = (ui16)(uvFloat.y * UINT16_MAX);
        myVert.normalPacked = Pack_INT_2_10_10_10_REV(normalFloat.x, normalFloat.y, normalFloat.z, 0.0f);
        myVert.tangentPacked = Pack_INT_2_10_10_10_REV(tangentFloat.x, tangentFloat.y, tangentFloat.z, 0.0f);
        myVert.color = COLOR_WHITE;
    }

    uploadMesh(vertices, indices16, PrimitiveShapeType::Sphere);
}

void PrimitiveShapeMeshes::generatePlaneMesh() {
    assert(false);
}

void PrimitiveShapeMeshes::generateCubeMesh() {
    assert(false);
}

// Chatgpt made this
void PrimitiveShapeMeshes::generateCylinderMesh() {
    constexpr float radius = 1.0f;
    constexpr float height = 1.0f;
    int numSides = 16;

    struct TmpVertex {
        f32v3 position;
        f32v3 normal;
        f32v3 tangent;
        f32v2 uv;
    };

    // The resulting vector of vertices
    std::vector<TmpVertex> vertices;

    // Calculate the angle between each side
    float angleBetweenSides = 360.0f / numSides;

    // Calculate the texture coordinate increment
    // between each side
    float uvIncrement = 1.0f / numSides;

    // Generate the vertices for the top and bottom disks
    for (int i = 0; i < numSides; i++) {
        // Calculate the angle for this side
        float angle = angleBetweenSides * i;

        // Calculate the x and y components of the position
        // using trigonometry
        float x = radius * std::cos(angle * 3.14159f / 180.0f);
        float y = radius * std::sin(angle * 3.14159f / 180.0f);

        // The normal for the top and bottom disks is just the
        // up vector (0, 0, 1) or (0, 0, -1), respectively
        f32v3 normal = { 0.0f, 0.0f, 1.0f };

        // The tangent is just the normalized side vector
        f32v3 tangent = { -y, x, 0.0f };
        tangent = glm::normalize(tangent);

        // The texture coordinate for this vertex is based on the
        // angle around the disk
        f32v2 uv = { uvIncrement * i, 0.0f };

        // Add the top vertex
        vertices.push_back({ {x, y, height / 2}, normal, tangent, uv });

        // Flip the normal and uv.y for the bottom vertex
        normal = { 0.0f, 0.0f, -1.0f };
        uv.y = 1.0f;
        vertices.push_back({ {x, y, -height / 2}, normal, tangent, uv });
    }
    // Generate the vertices for the sides of the cylinder
    for (int i = 0; i < numSides; i++) {
        // Calculate the angle for this side
        float angle = angleBetweenSides * i;

        // Calculate the x and y components of the position
        // using trigonometry
        float x = radius * std::cos(angle * 3.14159f / 180.0f);
        float y = radius * std::sin(angle * 3.14159f / 180.0f);

        // The normal for the side quad is just the side vector
        f32v3 normal = { x, y, 0.0f };
        normal = glm::normalize(normal);

        // The tangent is the cross product of the normal and the up vector (0, 0, 1). This will give us
        // a vector that is tangent to the surface and points
        // in the positive x direction.
        f32v3 tangent = glm::cross(normal, { 0.0f, 0.0f, 1.0f });
        // The texture coordinate for this vertex is based on the
        // angle around the cylinder
        f32v2 uv = { uvIncrement * i, 0.0f };

        // Add the top vertex
        vertices.push_back({ {x, y, height / 2}, normal, tangent, uv });

        // Flip the uv.y for the bottom vertex
        uv.y = 1.0f;
        vertices.push_back({ {x, y, -height / 2}, normal, tangent, uv });
    }

    assert(false);
}


void PrimitiveShapeMeshes::uploadMesh(const std::vector<StaticModelVertex>& vertices, const std::vector<ui16>& indices16, PrimitiveShapeType shapeType) {

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    MeshLODData lodData;
    lodData.mTotalIndexCount = indices16.size();
    lodData.mLODStarts[1] = lodData.mTotalIndexCount;
    ModelMeshBuilder::uploadCpuMeshToGpu(vertices.data(), (ui32)vertices.size(), StaticModelVertex::vertexType(), indices16.data(), MeshIndexType::SHORT, lodData, mesh->mMainMesh);

    mMeshes[e_cast(shapeType)] = std::move(mesh);
}
