#pragma once

#include "world/IWorld.h"
#include "world/srv/SrvWorldInterface.h"

class SrvWorld : public IWorld, public SrvWorldInterface
{
    friend class WorldFactory;
protected:
    SrvWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(chunkGrid, heightmapGrid) {}

public:

    void onWorldBegin(const f32v2& loadCenter) override;

    //// Nav graph
    //std::unique_ptr<NavWorld> mNavWorld;

    //// TODO: Combine factory with ecs?
    //// ECS
    //std::unique_ptr<EntityComponentSystem> mEcs;

    //// Cities
    //std::unique_ptr<CityGraph> mCities;

    //// Structures
    //std::unique_ptr<StructureManager> mStructureManager;

    //// Stockpiles
    //std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    //// Physics
    //std::unique_ptr<PhysicsWorld> mPhysWorld;



    void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) override;
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) override;

};
