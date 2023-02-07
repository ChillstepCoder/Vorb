#pragma once

#include "rendering/mesh/Mesh.h"

struct StaticModelVertex;

enum class PrimitiveShapeType {
    IcoSphere,
    UVSphere,
    Plane,
    Cube,
    Cylinder,
    COUNT
};

// Class that will lazily generate and return meshes on demand, for use in editor/debug
static class PrimitiveShapeMeshes {
public:
    PrimitiveShapeMeshes() = delete;

    // Returns a newly or previously generated primitive mesh with normalized scale
    static Mesh& getOrGenerateShapeMesh(PrimitiveShapeType type);

private:
    static void generateIcoSphereMesh();
    static void generateUVSphereMesh();
    static void generatePlaneMesh();
    static void generateCubeMesh();
    static void generateCylinderMesh();
    static void uploadMesh(const std::vector<StaticModelVertex>& vertices, const std::vector<ui16>& indices16, PrimitiveShapeType shapeType);

    inline static std::unique_ptr<Mesh> mMeshes[e_cast(PrimitiveShapeType::COUNT)];

};

