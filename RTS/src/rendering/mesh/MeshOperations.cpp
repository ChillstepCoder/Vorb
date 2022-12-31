#include "stdafx.h"
#include "MeshOperations.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/Vertex.h"

void MeshOperations::rotate90AboutAxis(MeshCpuData& mesh, const f32v3& axis) {
    assert(false);
}

void MeshOperations::applyScale(MeshCpuData& mesh, f32 scale) {
    switch (mesh.mVertexType) {
        case VertexType::STANDARD:
            for (ui32 i = 0; i < mesh.mVertsCount; ++i) {
                static_cast<StandardVertex*>(mesh.mVertsPtr)[i].pos *= scale;
            }
            break;
        case VertexType::STATIC_MODEL:
            for (ui32 i = 0; i < mesh.mVertsCount; ++i) {
                static_cast<StaticModelVertex*>(mesh.mVertsPtr)[i].pos *= scale;
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
    static_assert(e_cast(VertexType::COUNT) == 6);
}
