#pragma once

#include "rendering/mesh/Vertex.h"
#include "Mesh.h"
// TODO: Why is FbxMesh ambiguous if we forward declare instead?
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

struct SubTexture;
struct RawSubMesh;
struct RawMaterialData;
class SkinnedMesh; // TODO: Just mesh?
class SkinnedModel3D;
class StaticModel3D;
class MaterialRepository;
typedef ozz::animation::offline::fbx::FbxSceneLoader OzzFbxSceneLoader;

// Keep track of all they types of indices so we can decide to share if needed
enum class ModelMeshVertexType : ui8 {
    STATIC,
    SKINNED
};

class ModelMeshBuilder
{
public:
    // TODO: Instead optional skeleton and modelDef?
    bool buildSkinnedMeshesForModel(
        SkinnedModel3D& model,
        const ozz::animation::Skeleton& skeleton,
        const vio::Path& filePath,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode,
        const MaterialRepository& materialRepo
    );
    static MeshCpuData buildRuntimeOptimizedMeshFromRawMesh(
        RawSubMesh& subMesh,
        const std::vector<RawMaterialData>& rawMaterials,
        const MaterialRepository& materialRepo,
        ui8 numSkinningMatrices
    );
    static void uploadCpuMeshToGpu(const MeshCpuData& cpuMesh, MeshGpuData& outGpuMesh);

private:
    std::vector<Vertex32> mStaticVerts;
    std::vector<Vertex64> mSkinnedVerts;
    std::vector<uint16_t> mIndices;
};

