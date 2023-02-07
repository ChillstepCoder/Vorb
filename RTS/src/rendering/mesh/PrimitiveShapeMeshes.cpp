#include "stdafx.h"
#include "PrimitiveShapeMeshes.h"

#include "Vorb/MeshGenerators.h"

#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
#include "rendering/mesh/mesher/builder/MeshBuilderCommon.h"

#include "glm/gtx/rotate_vector.hpp"

Mesh& PrimitiveShapeMeshes::getOrGenerateShapeMesh(PrimitiveShapeType type) {
    assert(IS_RENDER_THREAD());

    // Generate lazily if needed
    if (mMeshes[e_cast(type)] == nullptr) {
        switch (type) {
            case PrimitiveShapeType::IcoSphere:
                generateIcoSphereMesh();
                break;
            case PrimitiveShapeType::UVSphere:
                generateUVSphereMesh();
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
        static_assert(e_cast(PrimitiveShapeType::COUNT) == 5);
    }

    return *mMeshes[e_cast(type)];
}

void PrimitiveShapeMeshes::generateIcoSphereMesh() {
    std::vector<ui32> indices;
    std::vector<f32v3> positions;
    vmesh::generateIcosphereMesh(3, indices, positions);
    std::vector<ui16> indices16(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices16[i] = indices[i];
    }

    // This has an ugly seam
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
        uvFloat.x = atan2(normalFloat.x, 1.0 - normalFloat.y) / (M_2_PI) + 0.5;
        uvFloat.y = normalFloat.z * 0.5 + 0.5;
        uvFloat = glm::clamp(uvFloat, f32v2(0.0), f32v2(1.0));
        myVert.uvsPacked = PackUVs(uvFloat);
        myVert.normalPacked = Pack_INT_2_10_10_10_REV(normalFloat.x, normalFloat.y, normalFloat.z, 0.0f);
        myVert.tangentPacked = Pack_INT_2_10_10_10_REV(tangentFloat.x, tangentFloat.y, tangentFloat.z, 0.0f);
        myVert.color = COLOR_WHITE;
    }

    uploadMesh(vertices, indices16, PrimitiveShapeType::IcoSphere);
}

// http://www.songho.ca/opengl/gl_sphere.html
void PrimitiveShapeMeshes::generateUVSphereMesh() {

    constexpr int stackCount = 24;
    constexpr int sectorCount = stackCount * 2;
    constexpr float radius = 1.0f;

    // clear memory of prev arrays
    // TODO: Reserve
    std::vector<StaticModelVertex> vertices;

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * M_PIF / sectorCount;
    float stackStep = M_PIF / stackCount;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stackCount; ++i)
    {
        stackAngle = M_PIF / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi
            
            StaticModelVertex& v = vertices.emplace_back();

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = 1.0 - (float)i / stackCount;

            // Compute tangent http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
            f32v3 normal = f32v3(nx, ny, nz);
            f32v3 tangent = glm::rotate(normal, glm::radians(90.0f), f32v3(0.0f, 0.0f, 1.0f));

            v.build(f32v3(x, y, z), f32v3(nx, ny, nz), tangent, f32v2(s, t), COLOR_WHITE, 0, 0);
        }
    }
    // generate CCW index list of sphere triangles
    // k1--k1+1
    // |  / |
    // | /  |
    // k2--k2+1
    std::vector<ui16> indices;
    int k1, k2;
    for (int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }

        }
    }

    uploadMesh(vertices, indices, PrimitiveShapeType::UVSphere);
}

void PrimitiveShapeMeshes::generatePlaneMesh() {
    std::vector<StaticModelVertex> verts(4, StaticModelVertex{});
    std::vector<ui16> indices(6);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 2;
    indices[4] = 3;
    indices[5] = 0;

    const ui32 normalPacked = Pack_INT_2_10_10_10_REV(f32v3(0.0f, -1.0f, 0.0f));
    const ui32 tangentPacked = Pack_INT_2_10_10_10_REV(f32v3(1.0f, 0.0f, 0.0f));

    verts[0].pos = f32v3(-1.0f, 0.0f, -1.0f);
    verts[0].uvsPacked = PackUVs(f32v2(0.0f, 0.0f));
    verts[1].pos = f32v3(1.0f, 0.0f, -1.0f);
    verts[1].uvsPacked = PackUVs(f32v2(1.0f, 0.0f));
    verts[2].pos = f32v3(1.0f, 0.0f, 1.0f);
    verts[2].uvsPacked = PackUVs(f32v2(1.0f, 1.0f));
    verts[3].pos = f32v3(-1.0f, 0.0f, 1.0f);
    verts[3].uvsPacked = PackUVs(f32v2(0.0f, 1.0f));

    for (int i = 0; i < 4; ++i) {
        verts[i].normalPacked = normalPacked;
        verts[i].tangentPacked = tangentPacked;
        verts[i].color = COLOR_WHITE;
    }

    uploadMesh(verts, indices, PrimitiveShapeType::Plane);
}

void PrimitiveShapeMeshes::generateCubeMesh() {
    std::vector<StaticModelVertex> verts(6 * 4, StaticModelVertex{});
    std::vector<ui16> indices(6 * 6);

    // UVs
    for (size_t i = 0; i < verts.size(); i += 4) {
        verts[i].uvsPacked = PackUVs(f32v2(0.0f, 0.0f));
        verts[i + 1].uvsPacked = PackUVs(f32v2(1.0f, 0.0f));
        verts[i + 2].uvsPacked = PackUVs(f32v2(1.0f, 1.0f));
        verts[i + 3].uvsPacked = PackUVs(f32v2(0.0f, 1.0f));
    }

    // Positions normals and tangents
    for (int i = 0; i < (int)CubeFacing::COUNT; ++i) {
        const int vi = i * 4;
        const ui32 packedNormal = Pack_INT_2_10_10_10_REV(f32v3(CUBE_FACING_NORMALS[i]));
        const ui32 packedTangent = Pack_INT_2_10_10_10_REV(f32v3(CUBE_FACING_TANGENTS_3D[i]));
        verts[vi].normalPacked = packedNormal;
        verts[vi + 1].normalPacked = packedNormal;
        verts[vi + 2].normalPacked = packedNormal;
        verts[vi + 3].normalPacked = packedNormal;
        verts[vi].tangentPacked = packedTangent;
        verts[vi + 1].tangentPacked = packedTangent;
        verts[vi + 2].tangentPacked = packedTangent;
        verts[vi + 3].tangentPacked = packedTangent;
        verts[vi].pos = CUBE_POSITIONS[i][0];
        verts[vi + 1].pos = CUBE_POSITIONS[i][1];
        verts[vi + 2].pos = CUBE_POSITIONS[i][2];
        verts[vi + 3].pos = CUBE_POSITIONS[i][3];
        verts[vi].color = COLOR_WHITE;
        verts[vi + 1].color = COLOR_WHITE;
        verts[vi + 2].color = COLOR_WHITE;
        verts[vi + 3].color = COLOR_WHITE;
    }

    // Indices
    int j = 0;
    for (size_t i = 0; i < verts.size(); i += 4) {
        indices[j++] = i;
        indices[j++] = i + 1;
        indices[j++] = i + 2;
        indices[j++] = i + 2;
        indices[j++] = i + 3;
        indices[j++] = i;
    }


    uploadMesh(verts, indices, PrimitiveShapeType::Cube);
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

    // The resulting index buffer
    std::vector<ui16> indices;

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

    // Generate the indices for the top and bottom disks
    for (int i = 0; i < numSides - 1; i++) {
        indices.push_back(i * 2);
        indices.push_back(i * 2 + 1);
        indices.push_back(i * 2 + 3);
        indices.push_back(i * 2 + 2);
        indices.push_back(i * 2);
        indices.push_back(i * 2 + 3);
    }
    // Add the final indices for the top and bottom disks
    indices.push_back((numSides - 1) * 2);
    indices.push_back((numSides - 1) * 2 + 1);
    indices.push_back(1);
    indices.push_back(0);
    indices.push_back((numSides - 1) * 2);
    indices.push_back(1);

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
        // a vector that is tangent to the surface and points in the positive x direction.
        f32v3 tangent = glm::cross(normal, { 0.0f, 0.0f, 1.0f });

        // The texture coordinate for this vertex is based on the angle around the cylinder
        f32v2 uv = { uvIncrement * i, 0.0f };

        // Add the top vertex
        vertices.push_back({ {x, y, height / 2}, normal, tangent, uv });

        // Flip the uv.y for the bottom vertex
        uv.y = 1.0f;
        vertices.push_back({ {x, y, -height / 2}, normal, tangent, uv });
    }

    // Generate the indices for the sides of the cylinder
    for (int i = 0; i < numSides - 1; i++) {
        // The indices for the two triangles that make up this quad
        uint32_t i1 = (numSides * 2) + (i * 2);
        uint32_t i2 = (numSides * 2) + (i * 2 + 1);
        uint32_t i3 = (numSides * 2) + (i * 2 + 3);
        uint32_t i4 = (numSides * 2) + (i * 2 + 2);

        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
        indices.push_back(i4);
        indices.push_back(i1);
        indices.push_back(i3);
    }
    // Add the final indices for the sides of the cylinder
    uint32_t i1 = (numSides * 2) + (numSides - 1) * 2;
    uint32_t i2 = (numSides * 2) + (numSides - 1) * 2 + 1;
    uint32_t i3 = (numSides * 2) + 1;
    uint32_t i4 = (numSides * 2) + 0;
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i3);
    indices.push_back(i4);
    indices.push_back(i1);
    indices.push_back(i3);

    std::vector<StaticModelVertex> staticVertices(vertices.size());
    for (size_t i = 0; i < staticVertices.size(); ++i) {
        StaticModelVertex& myVert = staticVertices[i];
        myVert.pos = vertices[i].position;
        myVert.uvsPacked.x = (ui16)(vertices[i].uv.x * UINT16_MAX);
        myVert.uvsPacked.y = (ui16)(vertices[i].uv.y * UINT16_MAX);
        myVert.normalPacked = Pack_INT_2_10_10_10_REV(vertices[i].normal);
        myVert.tangentPacked = Pack_INT_2_10_10_10_REV(vertices[i].tangent);
        myVert.color = COLOR_WHITE;
    }

    uploadMesh(staticVertices, indices, PrimitiveShapeType::Cylinder);
}


void PrimitiveShapeMeshes::uploadMesh(const std::vector<StaticModelVertex>& vertices, const std::vector<ui16>& indices16, PrimitiveShapeType shapeType) {

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();

    MeshLODData lodData;
    lodData.mTotalIndexCount = indices16.size();
    lodData.mLODStarts[1] = lodData.mTotalIndexCount;
    ModelMeshBuilder::uploadCpuMeshToGpu(vertices.data(), (ui32)vertices.size(), StaticModelVertex::vertexType(), indices16.data(), MeshIndexType::SHORT, lodData, mesh->mMainMesh);

    mMeshes[e_cast(shapeType)] = std::move(mesh);
}
