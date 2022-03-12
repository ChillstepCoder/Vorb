#pragma once

#include "rendering/TriangleMesh.h"

class SkinnedModel3D {
    friend class ModelRepository;
public:
    const SkinnedMesh* getMeshes() const { return mMeshes.get(); }
    ui32 getNumMeshes() const { return mNumMeshes; }
    ui8 getNumSkinningMatrices() const { return mNumSkinningMatrices; }

private:
    std::unique_ptr<SkinnedMesh[]> mMeshes;
    ui32 mNumMeshes = 0;
    ui8 mNumSkinningMatrices;
};

