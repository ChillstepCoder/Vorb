#include "stdafx.h"
#include "MeshOperations.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/Vertex.h"
#include "rendering/mesh/FBXRawModel.h"

void MeshOperations::rotate90AboutAxis(MeshCpuData& mesh, const f32v3& axis) {
    assert(false);
}

void MeshOperations::applyScale(MeshCpuData& mesh, f32 scale) {
    switch (mesh.mVertexType) {
        case VertexType::STANDARD_MODEL:
            for (ui32 i = 0; i < mesh.mVertsCount; ++i) {
                static_cast<StandardModelVertex*>(mesh.mVertsPtr)[i].pos *= scale;
            }
            break;
        case VertexType::SKINNED_MODEL:
            for (ui32 i = 0; i < mesh.mVertsCount; ++i) {
                static_cast<SkinnedModelVertex*>(mesh.mVertsPtr)[i].pos *= scale;
            }
            break;
        case VertexType::TERRAIN:
        case VertexType::WATER:
        default:
            assert(false && "Unsupported applyScale operation");
    }
    static_assert(e_cast(VertexType::COUNT) == 5);
}

void MeshOperations::setAllNormals(FBXRawModel& rawMesh, const f32v3& normal, const f32v3& tangent) {
    for (auto& [renderPass, submeshList] : rawMesh.mSubMeshes) {
        for (auto&& subMesh : submeshList) {
            for (auto&& v : subMesh.mVertices) {
                v.normal = normal;
                v.tangent = tangent;
            }
        }
    }
}
