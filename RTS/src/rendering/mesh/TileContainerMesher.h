#pragma once

class TileContainer;
class PhysicsWorld;

class TileContainerMesher
{
public:
    static void initMeshAndPhysicsAsync(TileContainer& chunk, const f32* heightData);

};

