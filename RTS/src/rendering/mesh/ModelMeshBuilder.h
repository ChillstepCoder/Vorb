#pragma once

#include "rendering/model/ModelVertex.h"
#include "Mesh.h"
// TODO: Why is FbxMesh ambiguous if we forward declare instead?
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>


struct SubTexture;
class SkinnedMesh; // TODO: Just mesh?
typedef ozz::animation::offline::fbx::FbxSceneLoader OzzFbxSceneLoader;

// Keep track of all they types of indices so we can decide to share if needed
enum class ModelMeshVertexType : ui8 {
    STATIC,
    SKINNED
};

class ModelMeshBuilder
{
public:
    bool buildStaticMesh(
        Mesh& outMesh,
        int meshIndex,
        const vio::Path& filePath,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode
    );
    bool buildSkinnedMesh(
        SkinnedMesh& outMesh,
        int meshIndex,
        const ozz::animation::Skeleton& skeleton,
        const vio::Path& filePath,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode
    );

private:
    void uploadStaticMeshData(Mesh& outMesh);

    std::vector<StaticModelVertex> mStaticVerts;
    std::vector<SkinnedModelVertex> mSkinnedVerts;
};

