#pragma once
#include "rendering/mesh/mesher/ITileContainerMesher.h"

class TileContainer;

class ChunkMesher : public ITileContainerMesher
{
public:
    ChunkMesher(TileContainerRenderer& renderer) : ITileContainerMesher(renderer) {}

    void initMeshAndPhysicsAsync(TileContainer& tileContainer);
};
