#pragma once

#include "world/IWorld.h"
#include "world/srv/SrvWorldInterface.h"

class DedicatedSrvWorld : public IWorld, public SrvWorldInterface
{
public:
    DedicatedSrvWorld(ui32 widthTiles, IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(widthTiles, chunkGrid, heightmapGrid) {}

    void onWorldBegin(const f32v2& loadCenter) override;

    void tick(f32 elapsedSec) override;

    WorldNetMode getNetMode() override;
    WorldType getWorldType() override;

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


    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) override;




};
