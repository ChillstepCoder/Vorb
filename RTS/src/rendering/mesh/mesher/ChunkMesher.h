#pragma once
#include "rendering/mesh/mesher/ITileContainerMesher.h"

class TileContainer;

class ChunkMesher : public ITileContainerMesher
{
public:
    ChunkMesher(TileContainerMeshManager& meshManager) : ITileContainerMesher(meshManager) {}

    void initMeshAndPhysicsAsync(TileContainer& tileContainer);
};
