#pragma once
#include "rendering/mesh/mesher/ITileContainerMesher.h"

class TileContainer;

class ChunkMesher : public ITileContainerMesher
{
public:
    void initMeshAndPhysicsAsync(TileContainer& chunk, const f32* heightData);
};

extern ChunkMesher sChunkMesher;