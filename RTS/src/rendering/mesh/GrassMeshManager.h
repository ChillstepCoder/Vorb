#pragma once

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
    const std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>>& getGrassQuadtrees() const { return mChunkGrassQuadtrees; }

private:
    std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>> mChunkGrassQuadtrees;
};

