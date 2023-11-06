#pragma once

class World;

class VisibilityManager
{
public:
    VisibilityManager(World& world);
    ~VisibilityManager();
private:
    World& mWorld;
};

