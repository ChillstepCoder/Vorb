#pragma once

#include "structure/Structure.h"
#include "world/IChunkGrid.h"

#include <shared_mutex>

class World;

typedef std::unordered_map<StructureID, std::unique_ptr<Structure>> StructureMap;
struct ChunkStructureData {
    std::vector<StructureID> containedStructures;
    std::unique_ptr<StructureID[]> dTileStructures = nullptr;
};
class StructureGrid {
public:
    StructureGrid(World& world);
    ~StructureGrid() = default;

    // Can fail if overlapping an existing structure
    Structure* tryMakeNewStructure(StructureType type, const i32AABB3& tileAABB, ui32 floorHeight, const BitArray& ownedDTiles);

    void debugRender();

    // TODO: Structure could be destroyed after return! We need a structureHandle?
    Structure* tryGetStructureAtWorldPos(TileCoord worldPos) const;
    const StructureMap& getStructures() const { ASSERT_GAME_THREAD(); return mStructures; }

private:
    void initEventHandlers();
    void addStructureToChunk(StructureID structureId, ChunkID id);

    World& mWorld;
    mutable std::shared_mutex mMutex;
    std::unique_ptr<ChunkStructureData[]> mChunkStructureData;
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