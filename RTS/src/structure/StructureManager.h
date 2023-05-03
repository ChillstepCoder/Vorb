#pragma once

#include "structure/Structure.h"
#include "world/IChunkGrid.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

class IWorld;

// TODO: 3D?
typedef bg::model::point<i32, 2, bg::cs::cartesian> StructureBoxPoint;
typedef bg::model::box<StructureBoxPoint> StructureBBox;

typedef std::unordered_map<StructureID, std::unique_ptr<Structure>> StructureMap;

struct StructureRegion {
    StructureBBox box;
    StructureID id;

    bool operator==(const StructureRegion& rhs) const {
        return (id == rhs.id) && (memcmp(&this->box, &rhs.box, sizeof(box)) == 0);
    }
};
// https://stackoverflow.com/questions/64179718/storing-or-accessing-objects-in-boost-r-tree
template <>
struct bgi::indexable<StructureRegion>
{
    typedef StructureBBox result_type;
    StructureBBox operator()(const StructureRegion& c) const { return c.box; }
};

class StructureManager
{
public:
    StructureManager(IWorld& world);
    ~StructureManager() = default;

    Structure* makeNewStructure(StructureType type, const i32AABB3& aabb, ui32 floorHeight);

    void debugRender();

    // TODO: non vector
    std::vector<Structure*> tryGetStructuresAtWorldPos(const i32v2& worldPos) const;
    const StructureMap& getStructures() const { ASSERT_GAME_THREAD(); return mStructures; }

private:
    void initEventHandlers();

    IWorld& mWorld;
    std::mutex mMutex;
    std::unordered_map<LiteChunkID, std::vector<StructureID>> mDormantStructures; // Structures who depend on multiple chunks can be duplicated here
    StructureMap mStructures;
    bgi::rtree<StructureRegion, bgi::quadratic<16>> mSpatialLookup;
    ChunkListeners mChunkEventListeners;
};

