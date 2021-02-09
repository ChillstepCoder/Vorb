#pragma once

#include <world/Chunk.h>
#include "Building.h"

class CityPlanner;
class CityBuilder;
class World;


enum class MarketStallType {
    PRODUCE,
    MEAT,
    FISH,
    CHEESE,
    WEAPONS,
    ARMOR,
    TRINKETS,
    CLOTHES,
    WOOD,
};

// Structures are standalone objects that are invariant, and cannot be entered, but can optionally be interacted with
enum class StructureType {
    SILO,
    LARGE_STATUE,

};

enum class FurnitureType {
    CHAIR,
    TABLE,
    BED,
    BARREL,
    DRAWERS,
    CUPBOARDS,
    DRESSER,
    STOVE,
    CHEST,
};

struct CityTileData {
    ui8 isOccupied : 1;
    ui8 isReserved : 1;
};

// 0 bits are free, 1 bits are reserved by existing buildings, roads, or terrain
struct CityChunkData {
    // TODO: Bits instead of bools!
    CityTileData mData[CHUNK_SIZE] = {};
    ui32 mFreeBitCount = CHUNK_SIZE;
};

struct Date {
    ui16 year;
    ui16 month;
    ui16 day;
};

struct Item {
    // Type?
    ui32 mCount;
};

// Requisition from one 
struct Requisition {
    //  TODO: Contiguous memory buffer, combine the vectors
    std::vector<Item> mRequestedItems;
    std::vector<Item> mFilledItems;
    Date mDesiredCompleteByDate;
    Date mCancelDate; // If we reach this date, the order is canceled and trust is lowered
    bool mAllowPartial; // If true, will attempt to send partial shipment at fill by date, and the rest when completed.
    bool mAllowEarly; // If true, may send early when completed
};

// See ProfessionType
enum class TaskType {
    SMITH,
    COOK,
    BOWMAKE,
    FLETCH,
    PATROL,
    HUNT,
    FISH,
    MINE,
    SMELT_ORE,
    CLOTHESMAKE,
    EXPLORE,
    PLANT_CROPS,
    HARVEST_CROPS,
    FORAGE,
    TEND_ANIMALS,
    HUNT_BOUNTY,
    CHOP_WOOD,
    CUT_PLANKS,
};

// Job for a worker
struct Task {
    // ActorId mWorker
    float mEstimatedDuration;
    // TaskType mTaskType;
    // bool mIsUrgent; instead just have an urgent list
};

class City
{
    friend class CityPlanner;
    friend class CityBuilder;

public:
    City(const ui32v2& cityCenterWorldPos, World& world);
    ~City();

    void update(float deltaTime);

    CityBuilder& getCityBuilder() { return *mCityBuilder; }
    CityPlanner& getCityPlanner() { return *mCityPlanner; }
    BuildingDescriptionRepository& getBuildingRepository() { return mBuildingRepository; }

private:
    void tick();

    World& mWorld;
    BuildingDescriptionRepository& mBuildingRepository;

    float mCurrentThreatLevel = 0.0f; //[0,100] 0-5 peaceful, 6-15 wary, 16-30 dangerous, 31-50 very dangerous, 51-70 extremely dangerous, 71+ critical danger
    float mCurrentPowerLevel = 0.0f;

    std::string mName;
    // All chunks that contain the city
    std::vector<Chunk*> mChunks;
    //std::vector<ActorId> mResidents;
    std::vector<Building> mBuildings;

    std::unique_ptr<CityBuilder> mCityBuilder;
    std::unique_ptr<CityPlanner> mCityPlanner;

    ui32v2 mCityCenterWorldPos;
};

struct CityGraph {
    std::vector<std::unique_ptr<City>> mNodes;
};
