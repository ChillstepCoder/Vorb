#pragma once

#include "rendering/TriangleMesh.h"

class Model3D {
    friend class ModelRepository;
public:

};

class SkinnedModel3D : public Model3D {
    friend class ModelRepository;
public:
    const SkinnedMesh* getMeshes() const { return mSkinnedMeshes.get(); }
    ui32 getNumMeshes() const { return mNumMeshes; }
    ui8 getNumSkinningMatrices() const { return mNumSkinningMatrices; }

private:
    std::unique_ptr<SkinnedMesh[]> mSkinnedMeshes;
    ui32 mNumMeshes = 0;
    ui8 mNumSkinningMatrices;
};

