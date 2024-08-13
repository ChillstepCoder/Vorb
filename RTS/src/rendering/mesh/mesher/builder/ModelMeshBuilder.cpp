#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/mesh/mesher/builder/MeshBuilderCommon.h"
#include "rendering/model/StaticModelInstance.h"

#include "rendering/mesh/FBXRawModel.h"
#include "rendering/mesh/Mesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include "resources/MaterialRepository.h"

#include "fbx/ozzFbxToMesh.hpp"
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/scene/geometry/fbxlayer.h>

MeshCpuData ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(RawSubMesh& subMesh, const std::vector<FBXRawMaterialData>& rawMaterials, f32 baseOptimizeErrorThreshold, std::vector<ui16>* rawMaterialIdToSlots) {

    MeshCpuData rv;
    MaterialRepository& materialRepo = MaterialRepository::get();

    // Optimize + LOD
    OptimizedCpuMeshData meshData = MeshBuilderCommon::optimizeMeshAndGenerateLODs(subMesh.mIndices, subMesh.mVertices, baseOptimizeErrorThreshold);
    rv.mLodData = meshData.lodData;

    // Grab material IDs from material names
    std::vector<MaterialID> defaultMaterials;
    defaultMaterials.resize(rawMaterials.size());
    for (int i = 0; i < rawMaterials.size(); ++i) {
        const StrToken materialName = StrToken(rawMaterials[i].materialName);
        if (rawMaterials[i].defaultMaterialDef) {
            defaultMaterials[i] = (MaterialID)rawMaterials[i].defaultMaterialDef->getID();
        }
        else {
            defaultMaterials[i] = materialRepo.getMaterialId(materialName);
        }
    }

    // Slots
    std::vector<ui16> rawMaterialIdSlotMapping;
    // If we have specified slots, use them
    if (rawMaterialIdToSlots) {
        rawMaterialIdSlotMapping = *rawMaterialIdToSlots;
    }
    else {
        rawMaterialIdSlotMapping.reserve(4);
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

            // Assign slot index
            ui16 materialSlot = UINT16_MAX;
            for (size_t slotIndex = 0; slotIndex < rawMaterialIdSlotMapping.size(); ++slotIndex) {
                if (rawMaterialIdSlotMapping[slotIndex] == rawVert.rawMaterialIndex) {
                    myVert.materialSlot = (ui16)slotIndex;
                    materialSlot = (ui16)slotIndex;
                    break;
                }
            }
            if (materialSlot == UINT16_MAX) [[unlikely]] {
                // Assign new slot
                if (rawMaterialIdSlotMapping.size() >= 4) [[unlikely]] {
                    panic("Submesh with more than 4 materials found");
                }
                materialSlot = (ui16)rawMaterialIdSlotMapping.size();
                rawMaterialIdSlotMapping.push_back(rawVert.rawMaterialIndex);
            }

            myVert.pos = rawVert.pos;

            static bool PRINT_ONCE = true;
            if (PRINT_ONCE) {
                LOG_CRITICAL("TODO: ALLOW NEGATIVE UVS");
                PRINT_ONCE = false;
            }
            assert(rawVert.uvs.x >= -UV_MAX_RANGE && rawVert.uvs.x <= UV_MAX_RANGE && rawVert.uvs.y >= -UV_MAX_RANGE && rawVert.uvs.y <= UV_MAX_RANGE);
            myVert.uvsPacked = PackUVs(rawVert.uvs);
            myVert.normalPacked = Pack_INT_2_10_10_10_REV(rawVert.normal.x, rawVert.normal.y, rawVert.normal.z, 0.0f);
            myVert.tangentPacked = Pack_INT_2_10_10_10_REV(rawVert.tangent.x, rawVert.tangent.y, rawVert.tangent.z, 0.0f);
            myVert.color = rawVert.color;
            memcpy(myVert.boneWeights, rawVert.boneWeights, sizeof(f32) * MAX_BONES_PER_VERTEX);
            memcpy(myVert.boneIDs, rawVert.boneIDs, sizeof(ui8) * MAX_BONES_PER_VERTEX);
        }
    }
    else {
        StandardModelVertex* verts = new StandardModelVertex[rv.mVertsCount];
        rv.mVertsPtr = verts;
        rv.mVertexType = VertexType::STANDARD_MODEL;

      
       
        for (int i = 0; i < rv.mVertsCount; ++i) {
            const RawMeshVertex& rawVert = meshData.vertices[i];
            StandardModelVertex& myVert = verts[i];

            // Assign slot index
            ui16 materialSlot = UINT16_MAX;
            for (size_t slotIndex = 0; slotIndex < rawMaterialIdSlotMapping.size(); ++slotIndex) {
                if (rawMaterialIdSlotMapping[slotIndex] == rawVert.rawMaterialIndex) {
                    myVert.materialSlot = (ui16)slotIndex;
                    materialSlot = (ui16)slotIndex;
                    break;
                }
            }
            if (materialSlot == UINT16_MAX) [[unlikely]] {
                // Assign new slot
                if (rawMaterialIdSlotMapping.size() >= 4) [[unlikely]] {
                    panic("Submesh with more than 4 materials found");
                }
                materialSlot = (ui16)rawMaterialIdSlotMapping.size();
                rawMaterialIdSlotMapping.push_back(rawVert.rawMaterialIndex);
            }

            myVert.build(
                rawVert.pos,
                rawVert.normal,
                rawVert.tangent,
                rawVert.uvs,
                rawVert.color,
                materialSlot,
                0,
                rawVert.damageZoneIndex
            );
        }
    }

    // Copy indices
    // TODO: Allow int?
    rv.mIndexType = MeshIndexType::USHORT;
    ui16* indices = new ui16[meshData.indices.size()];
    rv.mElementsPtr = (void*)indices;
    rv.mElementsCount = meshData.indices.size();
    assert(rv.mElementsCount);
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

    assert(vertsPtr);
    assert(indicesPtr);

    MeshBuilderCommon::initMeshBuffers(outGpuMesh, nullptr);
    assert(indexType == MeshIndexType::USHORT); // TODO: Support int
    outGpuMesh.mLODData = lodData;
    outGpuMesh.mVertexType = vertexType;
    assert(lodData.mTotalIndexCount && "Index count is included in lodData, even if there is no LOD");
    MeshBuilderCommon::uploadIndexData(outGpuMesh, (ui16*)indicesPtr, lodData.mTotalIndexCount, 0);
    MeshBuilderCommon::uploadVertexData(outGpuMesh, vertsPtr, vertsCount, getVertexSize(vertexType), 0);
    // TODO: Common util?
    VertexType mappedVertexType = VertexType::INVALID;
    switch (vertexType) {
        case VertexType::TERRAIN:
            mappedVertexType = TerrainVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::WATER:
            mappedVertexType = WaterVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::STANDARD_MODEL:
            mappedVertexType = StandardModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::SKINNED_MODEL:
            mappedVertexType = SkinnedModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        default:
            assert(false);
    }
    static_assert(e_cast(VertexType::COUNT) == 5);
    assert(mappedVertexType == vertexType);
    checkGlError("ModelMeshBuilder::uploadCpuMeshToGpu");
}
