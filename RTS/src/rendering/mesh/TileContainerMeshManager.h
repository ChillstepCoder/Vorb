#pragma once

#include "tile/TileContainerEvents.h"
#include "rendering/TileContainerMeshData.h"

#include <boost/container/flat_set.hpp>

class BuildingMesher;
class InstancedStaticModelManager;
class ContainerMeshBuilders;
class WorldRenderDataManager;
class World;

// Manages the mesh data and passes off to physics world
class TileContainerMeshManager
{
public:
    TileContainerMeshManager(World& world, InstancedStaticModelManager& instancedStaticModelManager);
    ~TileContainerMeshManager();

    void frameUpdate();

    void shutdown();

    // Call when tile container is generated/loaded
    void initModelsForTileContainer(std::span<TileIndex> modelTileIndices, const TileContainer& container);

    static void updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders);

    // Accessors
    TileContainerMeshData& getMeshDataForTileContainer(TileContainerID id) { return mTileContainerMeshData[id]; }
    const boost::container::flat_set<const Mesh*>& getStaticMeshes() const { return mStaticMeshes; }
    const boost::container::flat_set<const Mesh*>& getDynamicMeshes() const { return mDynamicMeshes; }
    const boost::container::flat_set<const Mesh*>& getBillboardMeshes() const { return mBillboardMeshes; }

private:
    void removeMeshesForData(TileContainerMeshData& meshData);
    void addStaticMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mStaticMeshes.insert(mesh); }
    void removeStaticMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mStaticMeshes.erase(mesh); }
    void addDynamicMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mDynamicMeshes.insert(mesh); }
    void removeDynamicMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mDynamicMeshes.erase(mesh); }
    void addBillboardMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mBillboardMeshes.insert(mesh); }
    void removeBillboardMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mBillboardMeshes.erase(mesh); }

    void initEventHandlers(World& world);
    void initTileContainerMesh(TileContainer& tileContainer);
    // Mesh management
    UnorderedFlatMap<TileContainerID, TileContainerMeshData> mTileContainerMeshData;
    moodycamel::ConcurrentQueue<TileContainerID> mTileContainersToRemove;

    // Mesh lists
    boost::container::flat_set<const Mesh*> mStaticMeshes;
    boost::container::flat_set<const Mesh*> mDynamicMeshes;
    boost::container::flat_set<const Mesh*> mBillboardMeshes;

    // Meshers
    std::unique_ptr<BuildingMesher> mBuildingMesher;

    // Events
    TileContainerListeners mTileContainerListeners;

    // Models
    InstancedStaticModelManager& mInstancedStaticModelManager;

    World& mWorld;
};

