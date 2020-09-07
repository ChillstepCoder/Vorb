#pragma once

class Chunk;

class ChunkMesher {
public:
    ChunkMesher();

    void createMesh(const Chunk& chunk);
    void updateSpritebatch(const Chunk& chunk);
};

