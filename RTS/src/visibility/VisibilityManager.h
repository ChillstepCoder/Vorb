#pragma once

class World;
class TileContainer;

#include "tile/TileContainerEvents.h"
#include "visibility/VisibilityGraph.h"

class VisibilityManager
{
public:
    VisibilityManager(World& world);
    ~VisibilityManager();

    void initContainerVisibility(TileContainer& container) const;
private:
    void initEvents();

    World& mWorld;

    std::unordered_map<TileContainerID, TileContainerVisibilityGraph> mVisibilityGraphs;
    TileContainerListeners mTileContainerEventListeners;
};

