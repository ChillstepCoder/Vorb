#pragma once

#include "world/ChunkGridEvent.h"

class TileContainer;
class World;
class Building;

// Loads tile containers from disk or generates them from sim data, and initializes them with navmesh, visibility, mesh, ect.
class TileContainerLoader
{
public:
    TileContainerLoader(World& world);

    void loadBuildingAsync(Building& building) const;

private:
    void loadBuildingFromBlueprintAsync(Building& building) const;
    // Container must be initialized
    void loadChunkFromSimChunk(TileContainer& container) const;

    void initEvents();

    World& mWorld;
    ChunkGridListeners mChunkGridListeners;
};

