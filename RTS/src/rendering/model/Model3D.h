#pragma once

#include "rendering/mesh/Mesh.h"

// TODO: Batching location - See ModelDef::ModelDrawInfo
class Model3D {
    friend class ModelRepository;
    friend class ModelMeshBuilder;
public:
    const Mesh* getMesh() const { return mMesh.get(); }
    ui8 getNumJoints() const { return mNumJoints; }
    bool isSkinnedMesh() const { return mNumJoints > 0; }
private:
    std::unique_ptr<Mesh> mMesh;
    ui32 mNumJoints = 0;
};
