#include "stdafx.h"
#include "ModelDef.h"


void ModelDef::addMesh(std::unique_ptr<Mesh> mesh) {
    mMeshes.emplace_back(std::move(mesh));
}
