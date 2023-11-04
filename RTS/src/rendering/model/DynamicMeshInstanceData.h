#pragma once

class Mesh;

class DynamicMeshInstanceData
{
public:
    std::unique_ptr<GLIndirectBuffer> mDrawCommands;
    const Mesh* mMesh = nullptr;
};

// Stores all specific instances of a given model in the world
typedef std::map<ModelID, DynamicMeshInstanceData> DynamicModelInstanceMap;