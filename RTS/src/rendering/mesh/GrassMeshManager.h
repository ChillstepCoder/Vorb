#pragma once


#include "boost/container/flat_set.hpp"
#include "tile/TileContainer.h"

class ChunkGrassQuadtree;
class Chunk;
class GrassMeshManager
{
public:
    GrassMeshManager();
    ~GrassMeshManager();

    void tick();
    void addGrassForChunk(const Chunk& chunk);
    void removeGrassForChunk(const Chunk& chunk);
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius);

    const std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>>& getGrassQuadtrees() const { return mChunkGrassQuadtrees; }

private:
    std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>> mChunkGrassQuadtrees;
    boost::container::flat_map<TileContainerID, TileContainerEventDispatcher::Handle> mEditEventHandles;
};

