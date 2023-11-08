#pragma once

class World;
class TileContainer;

class VisibilityManager
{
public:
    VisibilityManager(World& world);
    ~VisibilityManager();

    void initContainerVisibility(TileContainer& container) const;
private:
    World& mWorld;
};

