#pragma once

#include "rendering/TriangleMesh.h"

class Model3D {
    friend class ModelRepository;
public:
    const SkinnedMesh* getMeshes() const { return mMeshes.get(); }
    ui32 getNumMeshes() const { return mNumMeshes; }

private:
    std::unique_ptr<SkinnedMesh[]> mMeshes;
    ui32 mNumMeshes = 0;
};

