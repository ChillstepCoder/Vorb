#pragma once

class TileContainer;
class World;

typedef std::function<void(TileContainer&)> TileContainerLoadFinishedCallback;

// Loads tile containers from disk or generates them, and initializes them with navmesh, visibility, mesh, ect.
class TileContainerLoader
{
public:
    TileContainerLoader(World& world);

    // Container must be initialized
    void loadChunkFromSimChunk(TileContainer& container);

private:
    std::unordered_map<TileContainerID, TileContainerLoadFinishedCallback> mLoadingContainers;
    World& mWorld;
};

