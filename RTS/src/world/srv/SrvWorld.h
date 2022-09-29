#pragma once

#include "world/IWorld.h"
#include "world/srv/SrvWorldInterface.h"

class SrvWorld : public IWorld, public SrvWorldInterface
{
public:
    SrvWorld();
    //// Nav graph
    //std::unique_ptr<NavWorld> mNavWorld;

    //// TODO: Combine factory with ecs?
    //// ECS
    //std::unique_ptr<EntityComponentSystem> mEcs;

    //// Factories
    //std::unique_ptr<EntityFactory> mEntityFactory;

    //// Cities
    //std::unique_ptr<CityGraph> mCities;

    //// Structures
    //std::unique_ptr<StructureManager> mStructureManager;

    //// Stockpiles
    //std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    //// Physics
    //std::unique_ptr<PhysicsWorld> mPhysWorld;

};
