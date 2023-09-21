#include "stdafx.h"
#include "ModelDef.h"


void ModelDef::addMesh(std::unique_ptr<Mesh>&& mesh) {
    assert(mNumMeshes < MAX_MODEL_MESH_COUNT);
    mMeshes[mNumMeshes++] = std::move(mesh);
}
