#pragma once
// Lets us query what objects are at a tile that we can interact with or inspect.
 // TODO: Will receive notifications if that tile changes so it can refresh

class World;
class ItemStockpile;
class Building;
class Chunk;

#include "world/TileHandle.h"


class WorldObjectQuery {
    friend class UIInteractMenuPopup;
public:
    WorldObjectQuery(World& world, ui32v2& tilePos);

    ItemStockpile* getStockpile() const { return mStockpileAtTile; }
    Building* getBuilding() const { return mBuildingAtTile; }
    const std::vector<entt::entity>& getEntities() const { return mEntitiesAtTile; }
    TileHandle getTileHandle() const { return handle; }
    

private:
    ItemStockpile* mStockpileAtTile = nullptr;
    Building* mBuildingAtTile = nullptr;
    std::vector<entt::entity> mEntitiesAtTile;
    World& mWorld;
    TileHandle handle;
};