#pragma once

#include <world/Chunk.h>
#include "Building.h"

class CityPlotter;
class CityPlanner;
class CityBuilder;
class CityResidentManager;
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
//struct Task {
    // ActorId mWorker
    //float mEstimatedDuration;
    // TaskType mTaskType;
    // bool mIsUrgent; instead just have an urgent list
//};

// TODO: Give each city its own random number generator with own seed, so they will generate the same based on initial seed
// This way, cities are more predictable from a certain seed and can be easier debugged
class City
{
    friend class CityPlanner;
    friend class CityBuilder;
    friend class CityPlotter;
    friend class CityResidentManager;

public:
    City(const ui32v2& cityCenterWorldPos, World& world);
    ~City();

    void update(float deltaTime);

    CityBuilder& getCityBuilder() { return *mCityBuilder; }
    CityPlanner& getCityPlanner() { return *mCityPlanner; }
    CityPlotter& getCityPlotter() { return *mCityPlotter; }
    BuildingDescriptionRepository& getBuildingRepository() { return mBuildingRepository; }

    // Accessors
    const ui32v2& getCityCenterWorldPos() { return mCityCenterWorldPos; }

    // Mutators
    void addResidentToCity(entt::entity entity);
    void removeResidentFromCity(entt::entity entity); // TODO: Notify?

private:
    void tick();

    RoadID addRoad(CityRoad& road);

    World& mWorld;
    BuildingDescriptionRepository& mBuildingRepository;

    // TODO: CityGuardManager
    float mCurrentThreatLevel = 0.0f; //[0,100] 0-5 peaceful, 6-15 wary, 16-30 dangerous, 31-50 very dangerous, 51-70 extremely dangerous, 71+ critical danger
    float mCurrentPowerLevel = 0.0f;

    std::string mName;
    // All chunks that contain the city
    std::vector<Chunk*> mChunks;
    std::vector<Building> mBuildings;
    std::vector<CityRoad> mRoads;

    std::unique_ptr<CityPlotter> mCityPlotter;
    std::unique_ptr<CityPlanner> mCityPlanner;
    std::unique_ptr<CityBuilder> mCityBuilder;
    std::unique_ptr<CityResidentManager> mCityResidentManager;

    // City center dims is even so this will be bottom left most center tile
    ui32v2 mCityCenterWorldPos;
    ui32v4 mCityAABB; // x,y,w,h

    ui32 mPopulation = 0;
    ui32 mPopulationCapacityRemaining = 0;
};

struct CityGraph {
    std::vector<std::unique_ptr<City>> mNodes;
};
