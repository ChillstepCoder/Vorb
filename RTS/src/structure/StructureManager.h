#pragma once

#include "structure/Structure.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>

class World;

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<ui32, 2, bg::cs::cartesian> BoxPoint;
typedef bg::model::box<BoxPoint> BBox;

typedef std::vector<std::unique_ptr<Structure>> StructureList;

class StructureManager
{
public:
    StructureManager(World& world);
    ~StructureManager() = default;

    Structure* makeNewStructure(StructureType type, const ui32AABB3& aabb, ui32 floorHeight);

    const StructureList& getStructures() const { return mStructures; }

private:
    StructureList mStructures;
    bgi::rtree<std::pair<BBox, StructureID>, bgi::quadratic<16>> mSpatialLookup;
    World& mWorld;
};

