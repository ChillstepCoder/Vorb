#pragma once

#include "building/Building.h"
#include "world/IChunkGrid.h"

#include <shared_mutex>

class World;
class BuildingBlueprint;
class Building;

typedef std::unordered_map<BuildingID, std::unique_ptr<Building>> StructureMap;
struct ChunkBuildingData {
    bool isSimulated = true;
    std::vector<BuildingID> containedStructures;
    std::unique_ptr<BuildingID[]> dTileStructures = nullptr;
};
class BuildingGrid {
public:
    BuildingGrid(World& world);
    ~BuildingGrid() = default;

    void tick();

    // Can fail if overlapping an existing structure
    // Will consume bptr via move if successful
    Building* tryMakeNewBuilding(const i32AABB3& tileAABB, ui32 floorHeight, const BitArray& ownedDTiles, std::unique_ptr<BuildingBlueprint>& bptr);

    void debugRender();

    // TODO: Structure could be destroyed after return! We need a structureHandle?
    Building* tryGetStructureAtWorldPos(TileCoord worldPos) const;
    const StructureMap& getStructures() const { ASSERT_GAME_THREAD(); return mStructures; }
    const Building& getStructure(BuildingID id) const { ASSERT_GAME_THREAD(); return *mStructures.at(id); }

private:
    void initEventHandlers();
    void removeStructureFromDeactivateList(Building* structure);

    World& mWorld;
    mutable std::shared_mutex mMutex;
    std::unique_ptr<ChunkBuildingData[]> mChunkStructureData;
    std::vector<Building*> mDeactivatingStructures; // Structures that are waiting to free their tile containers
    StructureMap mStructures;
    ChunkGridListeners mChunkEventListeners;
};

// OLD Spatial lookup
// bgi::rtree<StructureRegion, bgi::quadratic<16>> mSpatialLookup;
//
//#include <boost/geometry.hpp>
//#include <boost/geometry/geometries/point.hpp>
//#include <boost/geometry/geometries/box.hpp>
//#include <boost/geometry/index/rtree.hpp>
//
//namespace bg = boost::geometry;
//namespace bgi = boost::geometry::index;
//typedef bg::model::point<i32, 2, bg::cs::cartesian> StructureBoxPoint;
//typedef bg::model::box<StructureBoxPoint> StructureBBox;
//struct StructureRegion {
//    StructureBBox box;
//    StructureID id;
//
//    bool operator==(const StructureRegion& rhs) const {
//        return (id == rhs.id) && (memcmp(&this->box, &rhs.box, sizeof(box)) == 0);
//    }
//};
//// https://stackoverflow.com/questions/64179718/storing-or-accessing-objects-in-boost-r-tree
//template <>
//struct bgi::indexable<StructureRegion>
//{
//    typedef StructureBBox result_type;
//    StructureBBox operator()(const StructureRegion& c) const { return c.box; }
//};