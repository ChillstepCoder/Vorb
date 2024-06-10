#pragma once

#include "tile/TileHarvestable.h"

class NavWorld;
struct CoarseNavNode;
struct CoarseNavGraph;
struct ContainerNavData;
struct LiteTileHandle;
class NavPath;

#include <boost/heap/priority_queue.hpp>

typedef ui16 CoarseAstarNodeID;
constexpr ui16 INVALID_COARSE_NODE_PARENT = UINT16_MAX;

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
typedef std::vector<const CoarseNavNode*> CoarseClosedList;

// TODO: Memory recycler for path memory
//class PathCache {
    
//};

// TODO: Also support flow path finding for large group movements, such as for moving in formation
class PathFinder {
public:
    PathFinder(const NavWorld& navWorld);

    bool generateFinePathSynchronous(const f32v3 start, const f32v3 goal, f32 targetRadiusSQ, OUT NavPath& path);
    bool generateCoarsePathSynchronous(const f32v3 start, const f32v3 goal, f32 targetRadiusSQ, OUT NavPath& path);

    //LiteTileHandle tryGenerateCoarsePathToClosestFreeHarvestableSynchronous(const f32v3& start, TileHarvestable harvestable, f32 maxDistance, OUT NavPath& path);

private:
    void coarseAstarEdgePropagate(const ContainerNavData& navData, const CoarseNavNode* navNode, const LiteTileHandle& tileHandle, const CoarseNavGraph& navGraph, const f32v3& goalPos, CoarseAstarNodeID parentId, f32 prevG);
    void clearCoarseClosedList();
    void finishCoarsePath(LiteTileHandle startHandle, LiteTileHandle goalHandle, CoarseAstarNodeID lastId, NavPath& path, bool reverse);

    CoarseOpenList mOpenList;
    CoarseClosedList mCoarseClosedList;
    CoarseAstarNodeID mTotalAstarNodes;
    const NavWorld& mNavWorld;
};

