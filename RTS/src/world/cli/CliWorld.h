#pragma once
class CliWorld
{
    // Clouds
   // std::unique_ptr<CloudManager> mCloudManager;

};


class LocalCliWorld : public CliWorld {

};

class RemoteCliWorld : public CliWorld {
    //// ECS
    //std::unique_ptr<EntityComponentSystem> mEcs;

    //// Factories
    //std::unique_ptr<EntityFactory> mEntityFactory;

    //// Structures
    //std::unique_ptr<StructureManager> mStructureManager;

    //// Stockpiles
    //std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    //// Physics
    //std::unique_ptr<PhysicsWorld> mPhysWorld;

};