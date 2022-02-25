#pragma once

#include "rendering/TriangleMesh.h"

class Model3D {
    friend class ModelRepository;
    friend class CharacterRenderer;
public:

private:
    std::unique_ptr<IndexedTriangleMesh[]> mMeshes;
    ui32 mNumMeshes = 0;
};

