#pragma once

class Chunk;
class PhysicsWorld;

class ChunkMesher
{
public:
    static void buildMeshAndPhysicsAsync(const Chunk& chunk, PhysicsWorld& physWorld, const f32* heightData);

};

