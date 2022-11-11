#pragma once

#include "rendering/TriangleMesh.h"

enum class Model3DType {
    STATIC,
    SKINNED
};

class StaticModel3D {
    friend class ModelRepository;
    friend class ModelMeshBuilder;
public:
    const Mesh* getMesh() const { return mMesh.get(); }

private:
    std::unique_ptr<Mesh> mMesh;
};

class SkinnedModel3D {
    friend class ModelRepository;
    friend class ModelMeshBuilder;
public:
    const SkinnedMesh* getMeshes() const { return mSkinnedMeshes.get(); }
    ui32 getNumMeshes() const { return mNumMeshes; }
    ui8 getNumSkinningMatrices() const { return mNumSkinningMatrices; }

private:
    std::unique_ptr<SkinnedMesh[]> mSkinnedMeshes;
    ui32 mNumMeshes = 0;
    ui8 mNumSkinningMatrices;
};

