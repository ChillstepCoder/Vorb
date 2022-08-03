#include "stdafx.h"
#include "NavPath.h"

#include "tile/TileHandle.h"

#include <boost/pool/singleton_pool.hpp>

struct nav_pool {};
using singleton_path_pool = boost::singleton_pool<nav_pool, sizeof(NavPath), boost::default_user_allocator_new_delete, std::mutex, 128u>;
struct path_point_pool {};
using singleton_point_pool = boost::singleton_pool<path_point_pool, sizeof(LiteTileHandle), boost::default_user_allocator_new_delete, std::mutex, 512>;

void NavPath::allocatePath(ui32 numPoints) {
    this->numPoints = numPoints;
    points = (LiteTileHandle*)singleton_point_pool::ordered_malloc(numPoints);
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
