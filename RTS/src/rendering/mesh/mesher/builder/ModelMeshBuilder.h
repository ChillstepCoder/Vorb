#pragma once

#include "rendering/mesh/Vertex.h"
#include "rendering/mesh/Mesh.h"
// TODO: Why is FbxMesh ambiguous if we forward declare instead?
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

#include <util/fixed_capacity_vector.h>

struct RawSubMesh;
struct FBXRawMaterialData;
class MaterialRepository;
typedef ozz::animation::offline::fbx::FbxSceneLoader OzzFbxSceneLoader;

// Keep track of all they types of indices so we can decide to share if needed
enum class ModelMeshVertexType : ui8 {
    STATIC,
    SKINNED
};

static class ModelMeshBuilder
{
public:
    ModelMeshBuilder() = delete;

    static MeshCpuData buildRuntimeOptimizedMeshFromRawMesh(
        RawSubMesh& subMesh,
        const std::vector<FBXRawMaterialData>& rawMaterials,
        f32 baseOptimizeErrorThreshold,
        ui16 variantStartIndex,
        std::experimental::fixed_capacity_vector<ui16, 4>* rawMaterialIdToSlots = nullptr // For static model loading
    );

    static void uploadCpuMeshToGpu(const MeshCpuData& cpuMesh, MeshGpuData& outGpuMesh);
    static void uploadCpuMeshToGpu(const void* vertsPtr, ui32 vertsCount, VertexType vertexType, const void* indicesPtr, MeshIndexType indexType, const MeshLODData& lodData, MeshGpuData& outGpuMesh);
};

