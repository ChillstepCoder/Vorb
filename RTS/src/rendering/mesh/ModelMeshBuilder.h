#pragma once

#include "rendering/model/ModelVertex.h"
#include "Mesh.h"
// TODO: Why is FbxMesh ambiguous if we forward declare instead?
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

struct SubTexture;
class SkinnedMesh; // TODO: Just mesh?
class SkinnedModel3D;
class TextureRepository;
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
    bool buildSkinnedMeshesForModel(
        SkinnedModel3D& model,
        const ozz::animation::Skeleton& skeleton,
        const vio::Path& filePath,
        const vio::Path& rootDir,
        OzzFbxSceneLoader& sceneLoader,
        MeshDrawMode drawMode,
        const TextureRepository& textureRepo
    );

private:
    void initStaticMeshBuffers(SubMeshData& subMesh);
    void uploadStaticMeshData(SubMeshData& subMesh, const uint16_t* indices, int indexCount, MeshDrawMode drawMode);
    void bindStaticVertexAttribs(SubMeshData& subMesh);

    std::vector<StaticModelVertex> mStaticVerts;
    std::vector<SkinnedModelVertex> mSkinnedVerts;
};

