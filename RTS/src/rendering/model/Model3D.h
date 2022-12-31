#pragma once

#include "rendering/mesh/Mesh.h"

enum class Model3DType {
    STATIC,
    SKINNED,
    COUNT
};

// TODO: Deprecate?
class StaticModel3D {
    friend class ModelRepository;
    friend class ModelMeshBuilder;
public:
    const Mesh* getMesh() const { return mMesh.get(); }

private:
    std::unique_ptr<Mesh> mMesh;
};

// TODO: Deprecate?
class SkinnedModel3D {
    friend class ModelRepository;
    friend class ModelMeshBuilder;
public:
    const Mesh* getMeshes() const { return mSkinnedMeshes.get(); }
    ui32 getNumMeshes() const { return mNumMeshes; }
    ui8 getNumSkinningMatrices() const { return mNumSkinningMatrices; }

private:
    std::unique_ptr<Mesh[]> mSkinnedMeshes;
    ui32 mNumMeshes = 0;
    ui8 mNumSkinningMatrices;
};

