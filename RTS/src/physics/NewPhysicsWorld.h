#pragma once
class World;

class NewPhysicsWorld
{
public:
    NewPhysicsWorld(World& world);
    ~NewPhysicsWorld();

private:

    World& mWorld;
};

