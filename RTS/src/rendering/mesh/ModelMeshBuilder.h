#pragma once

#include "rendering/mesh/Vertex.h"
#include "Mesh.h"
// TODO: Why is FbxMesh ambiguous if we forward declare instead?
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

struct SubTexture;
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
    bool buildStaticMeshesForModel(
        StaticModel3D& model,
        const vio::Path& filePath,
        const vio::Path& rootDir,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode,
        const MaterialRepository& materialRepo,
        float modelScale
    );
    bool buildSkinnedMeshesForModel(
        SkinnedModel3D& model,
        const ozz::animation::Skeleton& skeleton,
        const vio::Path& filePath,
        const vio::Path& rootDir,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode,
        const MaterialRepository& materialRepo
    );

    static void updateInstanceDataForStaticModel(const Mesh& mesh, VGBuffer instanceDataVbo);

private:

    std::vector<Vertex32> mStaticVerts;
    std::vector<Vertex64> mSkinnedVerts;
    std::vector<uint16_t> mIndices;
};

