#pragma once
class World;

class JPHPhysicsWorldContext;

class NewPhysicsWorld
{
public:
    NewPhysicsWorld(World& world);
    ~NewPhysicsWorld();

private:

    World& mWorld;
    std::unique_ptr<JPHPhysicsWorldContext> mContext;
};

