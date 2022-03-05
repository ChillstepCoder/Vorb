#pragma once

#include "rendering/TriangleMesh.h"

class Model3D {
    friend class ModelRepository;
    friend class CharacterRenderer;
public:

private:
    std::unique_ptr<SkinnedMesh[]> mMeshes;
    ui32 mNumMeshes = 0;
};

