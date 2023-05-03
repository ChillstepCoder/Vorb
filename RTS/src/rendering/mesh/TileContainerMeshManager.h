#pragma once

#include "tile/TileContainerEvents.h"
#include "rendering/TileContainerMeshData.h"

class BuildingMesher;
class ChunkMesher;
class InstancedStaticModelManager;
class ContainerMeshBuilders;
class WorldRenderDataManager;

class TileContainerMeshManager
{
public:
    TileContainerMeshManager(InstancedStaticModelManager& instancedStaticModelManager);
    ~TileContainerMeshManager();

    void frameUpdate(WorldRenderDataManager& renderDataManager);

    static void updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders);

    TileContainerMeshData& getMeshDataForTileContainer(TileContainerID id) { return mTileContainerMeshData[id]; }

private:
    void initEventHandlers();
    void updateTileContainerMesh(TileContainer& tileContainer);
    // Mesh management
    std::unordered_map<TileContainerID, TileContainerMeshData> mTileContainerMeshData;

    moodycamel::ConcurrentQueue<TileContainerID> mTileContainersToRemove;

    // Meshers
    std::unique_ptr<BuildingMesher> mBuildingMesher;
    std::unique_ptr<ChunkMesher> mChunkMesher;

    // Events
    TileContainerListeners mTileContainerListeners;

    // Models
    InstancedStaticModelManager& mInstancedStaticModelManager;
};

