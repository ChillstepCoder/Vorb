#pragma once

#include "NavPath.h"

class World;
struct NavNode;

#include <boost/heap/priority_queue.hpp>

typedef ui16 CoarseAstarNodeID;
constexpr ui16 INVALID_COARSE_NODE_PARENT = UINT16_MAX;
static_assert(sizeof(CoarseAstarNodeID) == sizeof(ui16), "Update invalid parent");
struct compareCoarseNode {
    bool operator()(const std::pair<f32, CoarseAstarNodeID>& n1, const std::pair<f32, CoarseAstarNodeID>& n2) const {
        if (n1.first > n2.first) {
            return true;
        }
        else if (n1.first < n2.first) {
            return false;
        }
        return n1.second > n2.second;
    }
};

// TODO: Is there a better choice?
typedef boost::heap::priority_queue<std::pair<f32, CoarseAstarNodeID>, boost::heap::compare<compareCoarseNode>> CoarseOpenList;
typedef std::vector<const NavNode*> CoarseClosedList;

// TODO: Memory recycler for path memory
//class PathCache {
    
//};

// TODO: Also support flow path finding for large group movements, such as for moving in formation
class PathFinder {
public:
    PathFinder() {};

    // Fine grid paths
    bool generatePathSynchronous(const World& world, const PathPoint& start, const PathPoint& goal, OUT NavPath& path);

    // Coarse grid paths
    bool generateCoarsePathSynchronous(const World& world, const PathPoint& start, const PathPoint& goal, OUT NavPath& path);

private:
    CoarseOpenList mOpenList;
    CoarseClosedList mCoarseClosedList;
};

