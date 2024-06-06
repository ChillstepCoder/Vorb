#include "stdafx.h"
#include "NavPath.h"

#include "tile/TileHandle.h"
#include "world/IHeightmapGrid.h"

#include <boost/pool/singleton_pool.hpp>

struct nav_pool {};
using singleton_path_pool = boost::singleton_pool<nav_pool, sizeof(NavPath), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 128u>;
struct path_point_pool {};
using singleton_point_pool = boost::singleton_pool<path_point_pool, sizeof(NavPathPoint), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 512>;

NavPath::NavPath(NavPath&& other) noexcept {
    numPoints = other.numPoints;
    points = other.points;
    other.numPoints = 0;
    other.points = nullptr;
}

NavPath& NavPath::operator=(NavPath&& other) noexcept {
    numPoints = other.numPoints;
    points = other.points;
    other.numPoints = 0;
    other.points = nullptr;
    return *this;
}

void NavPath::allocatePath(ui32 numPoints) {
    this->numPoints = numPoints;
    if (numPoints) {
        points = (NavPathPoint*)singleton_point_pool::ordered_malloc(numPoints);
    }
}

void NavPath::freePath() {
    if (numPoints) {
        singleton_point_pool::ordered_free(points, numPoints);
        points = nullptr;
        numPoints = 0;
    }
}

void* NavPath::operator new(size_t count) {
    UNUSED(count);
    return singleton_path_pool::malloc();
}

void NavPath::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return singleton_path_pool::free(pointer);
}

std::vector<f32v3> NavPath::convertToWorldPoints(const IHeightmapGrid& heightGrid) const
{
    // TODO: DEPRECATE THIS
    if (!points) return std::vector<f32v3>();
    // These two threads have will lock the world state so we are safe to read
    assert(IS_GAME_THREAD() || IS_NAV_THREAD());
    std::vector<f32v3> rv(numPoints);
    for (ui32 i = 0; i < numPoints; ++i) {
        rv[i] = points[i].pos;
    }
    return rv;
}
