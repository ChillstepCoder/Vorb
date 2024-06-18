#pragma once

#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshSkeletonData.h"
#include "rendering/material/MaterialData.h"

// Not intended to be uploaded to GPU except for editor render
struct alignas(16) RawMeshVertex {
    f32v3 pos;
    f32v3 normal; // TODO: Test uncompressed since we have lots of padding room
    f32v3 tangent;
    f32v2 uvs;
    color4 color;
    ui16 rawMaterialIndex;
    ui16 damageZoneIndex;
    f32 boneWeights[MAX_BONES_PER_VERTEX] = {}; // 0 Weight default 
    ui8 boneIDs[MAX_BONES_PER_VERTEX] = {}; //
};

// Loaded from a model Not intended to be uploaded to GPU except for editor render
struct FBXRawMaterialData {
    nString materialName;
    const MaterialDef* defaultMaterialDef = nullptr;
    f32v4 emissiveColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32v4 albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    f32v4 specularColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // UV anisotropic roughness (isotropic lighting models use only the first value). ZW values are ignored
    f32v4 roughness = { 1.0f, 1.0f, 0.0f, 0.0f };

    f32 transparencyFactor = 1.0f; // UNUSED
    f32 alphaTest = 0.01f;
    f32 metallicFactor = 0.0f;
    f32 bumpFactor = 1.0f;
    f32 emissiveFactor = 0.0f;
    f32 albedoFactor = 1.0f;

    ui32 flags = MaterialFlags_CastShadow | MaterialFlags_ReceiveShadow;
    // maps
    nString albedoTextureName;
    nString normalTextureName;
    nString ambientOcclusionTextureName;
    nString metallicRoughnessTextureName;
};

struct RawSubMesh {
    // We split all meshes based on render pass
    std::vector<RawMeshVertex> mVertices;
    std::vector<ui32> mIndices;
    RawMeshSkeletonData mSkeletonData;
    bool mHasSkin = false;
};

// Contains everything that a mesh could need, skeleton, vertex data, vertex types,
// can be exported or converted into proper GPU meshes.
class FBXRawMesh {
public:
    std::vector<FBXRawMaterialData> mMaterials;
    std::vector<RawSubMesh> mSubMeshes;
    RawSubMesh mCombinedMeshData[e_count(MaterialRenderPassType)];
};

