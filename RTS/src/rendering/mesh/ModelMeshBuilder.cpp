#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/mesh/MeshBuilderCommon.h"
#include "rendering/texture/SubTexture.h"
#include "rendering/model/StaticModelInstance.h"

#include "rendering/model/Model3D.h"
#include "rendering/mesh/RawMesh.h"
#include "rendering/mesh/Mesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include "resources/MaterialRepository.h"

#include "fbx/ozzFbxToMesh.hpp"
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/scene/geometry/fbxlayer.h>

MeshCpuData ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(RawSubMesh& subMesh, const std::vector<RawMaterialData>& rawMaterials, const MaterialRepository& materialRepo) {

    MeshCpuData rv;

    // Optimize + LOD
    OptimizedCpuMeshData meshData = MeshBuilderCommon::optimizeMeshAndGenerateLODs(subMesh.mIndices, subMesh.mVertices);
    rv.mLodData = meshData.lodData;

    // Grab material IDs from material names
    std::vector<MaterialID> materialIds;
    materialIds.resize(rawMaterials.size());
    for (int i = 0; i < rawMaterials.size(); ++i) {
        const nString materialName = rawMaterials[i].materialName;
        materialIds[i] = materialRepo.getMaterialId(materialName);
    }

    // Different vertex format based on skin or no
    rv.mVertsCount = meshData.vertices.size();
    if (subMesh.mHasSkin) {
        SkinnedModelVertex* verts = new SkinnedModelVertex[rv.mVertsCount];
        rv.mVertsPtr = verts;
        rv.mVertexType = VertexType::SKINNED_MODEL;
        for (int i = 0; i < rv.mVertsCount; ++i) {
            const RawMeshVertex& rawVert = meshData.vertices[i];
            SkinnedModelVertex& myVert = verts[i];
            myVert.pos = rawVert.pos;
            myVert.materialId = materialIds[rawVert.materialIndex];
            assert(rawVert.uvs.x >= 0.0f && rawVert.uvs.x <= 1.0f && rawVert.uvs.y >= 0.0f && rawVert.uvs.y <= 1.0f);
            myVert.uvsPacked.x = (ui16)(rawVert.uvs.x * UINT16_MAX);
            myVert.uvsPacked.y = (ui16)(rawVert.uvs.y * UINT16_MAX);
            myVert.normalPacked = Pack_INT_2_10_10_10_REV(rawVert.normal.x, rawVert.normal.y, rawVert.normal.z, 0.0f);
            myVert.tangentPacked = Pack_INT_2_10_10_10_REV(rawVert.tangent.x, rawVert.tangent.y, rawVert.tangent.z, 0.0f);
            myVert.color = rawVert.color;
            memcpy(myVert.boneWeights, rawVert.boneWeights, sizeof(f32) * MAX_BONES_PER_VERTEX);
            memcpy(myVert.boneIDs, rawVert.boneIDs, sizeof(ui8) * MAX_BONES_PER_VERTEX);
        }
    }
    else {
        StaticModelVertex* verts = new StaticModelVertex[rv.mVertsCount];
        rv.mVertsPtr = verts;
        rv.mVertexType = VertexType::STATIC_MODEL;
        for (int i = 0; i < rv.mVertsCount; ++i) {
            const RawMeshVertex& rawVert = meshData.vertices[i];
            StaticModelVertex& myVert = verts[i];
            myVert.pos = rawVert.pos;
            myVert.materialId = materialIds[rawVert.materialIndex];
            f32v2 uvs;
            uvs.x = glm::clamp(rawVert.uvs.x, 0.0f, 1.0f);
            uvs.y = glm::clamp(rawVert.uvs.y, 0.0f, 1.0f);
            //assert(rawVert.uvs.x >= 0.0f && rawVert.uvs.x <= 1.0f && rawVert.uvs.y >= 0.0f && rawVert.uvs.y <= 1.0f);
            myVert.uvsPacked.x = (ui16)(uvs.x * UINT16_MAX);
            myVert.uvsPacked.y = (ui16)(uvs.y * UINT16_MAX);
            myVert.normalPacked = Pack_INT_2_10_10_10_REV(rawVert.normal.x, rawVert.normal.y, rawVert.normal.z, 0.0f);
            myVert.tangentPacked = Pack_INT_2_10_10_10_REV(rawVert.tangent.x, rawVert.tangent.y, rawVert.tangent.z, 0.0f);
            myVert.color = rawVert.color;
        }
    }

    // Copy indices
    // TODO: Allow int?
    rv.mIndexType = MeshIndexType::SHORT;
    ui16* indices = new ui16[meshData.indices.size()];
    rv.mElementsPtr = (void*)indices;
    for (int i = 0; i < meshData.indices.size(); ++i) {
        assert(meshData.indices[i] <= UINT16_MAX);
        indices[i] = (ui16)meshData.indices[i];
    }

    return rv;
}

void ModelMeshBuilder::uploadCpuMeshToGpu(const MeshCpuData& cpuMesh, MeshGpuData& outGpuMesh) {
    return uploadCpuMeshToGpu(cpuMesh.mVertsPtr, cpuMesh.mVertsCount, cpuMesh.mVertexType, cpuMesh.mElementsPtr, cpuMesh.mIndexType, cpuMesh.mLodData, outGpuMesh);
}

void ModelMeshBuilder::uploadCpuMeshToGpu(const void* vertsPtr, ui32 vertsCount, VertexType vertexType, const void* indicesPtr, MeshIndexType indexType, const MeshLODData& lodData, MeshGpuData& outGpuMesh) {

    MeshBuilderCommon::initMeshBuffers(outGpuMesh, nullptr);
    assert(indexType == MeshIndexType::SHORT); // TODO: Support int
    outGpuMesh.mLODData = lodData;
    outGpuMesh.mVertexType = vertexType;
    assert(lodData.mTotalIndexCount && "Index count is included in lodData, even if there is no LOD");
    MeshBuilderCommon::uploadIndexData(outGpuMesh, (ui16*)indicesPtr, lodData.mTotalIndexCount, 0);
    MeshBuilderCommon::uploadVertexData(outGpuMesh, vertsPtr, vertsCount, getVertexSize(vertexType), 0);
    // TODO: Common util?
    VertexType mappedVertexType = VertexType::INVALID;
    switch (vertexType) {
        case VertexType::STANDARD:
            mappedVertexType = StandardVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::TERRAIN:
            mappedVertexType = TerrainVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::WATER:
            mappedVertexType = WaterVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::STATIC_MODEL:
            mappedVertexType = StaticModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::SKINNED_MODEL:
            mappedVertexType = SkinnedModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        default:
            assert(false);
    }
    static_assert(e_cast(VertexType::COUNT) == 6);
    assert(mappedVertexType == vertexType);
    checkGlError("ModelMeshBuilder::uploadCpuMeshToGpu");
}
