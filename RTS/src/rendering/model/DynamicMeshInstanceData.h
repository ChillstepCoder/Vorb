#pragma once

class Mesh;

class DynamicMeshInstanceData
{
public:
    mutable std::unique_ptr<GLDrawCommandBuffer> mDrawCommands;
    const Mesh* mMesh = nullptr;
};
