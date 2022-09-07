#include "stdafx.h"
#include "PathFinder.h"

#include "world/World.h"
#include "resources/TileRepository.h"

#include "pathfinding/NavWorld.h"

#include "options/DebugOptions.h"

#include "debugging/DebugRenderer.h"

#include "city/Building.h"

constexpr Cartesian CARTESIAN_COARSE_EDGE_WALK_CARTESIAN[4] = {
    Cartesian::EAST, //South
    Cartesian::NORTH, //West
    Cartesian::NORTH, //East
    Cartesian::EAST, //North
};

constexpr ui32 MAX_PATH_LENGTH = 128; //255;
constexpr ui8 INVALID_PARENT = 0;

constexpr ui32 MAX_OPEN_LIST_SIZE = MAX_PATH_LENGTH * 8;

constexpr ui32 DEBUG_DURATION = 200;
inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, worldGrid.tryComputeHeightAtPoint(pos2d));
}

struct CoarseAStarNode {
    LiteTileHandle tileHandle;
    f32 h; // Heuristic distance to target
    f32 g; // Movement cost to the node
    ui16 parentIndex;

    f32 getScore() const { return g + h; }
};

constexpr CoarseAstarNodeID MAXIMUM_COARSE_NODES = 8196;
thread_local CoarseAStarNode sCoarseAstarNodes[MAXIMUM_COARSE_NODES];

// No allocations baby
constexpr ui32 PATH_POINT_BUFFER_SIZE = MAX_PATH_LENGTH * 16;
thread_local LiteTileHandle sPathPointBuffer[PATH_POINT_BUFFER_SIZE];

typedef ui16 AStarNodeID;

struct compareFineNode {
    bool operator()(const std::pair<f32, LiteTileHandle>& n1, const std::pair<f32, LiteTileHandle>& n2) const {
        if (n1.first > n2.first) {
            return true;
        }
        else if (n1.first < n2.first) {
            return false;
        }
        // TODO: Do we need or care about these comparisons?
        if (n1.second.index > n2.second.index) {
            return true;
        } else if (n1.second.index < n2.second.index) {
            return false;
        }
        return n1.second.containerId < n2.second.containerId;
    }
};

// TODO: Is there a better choice?
typedef boost::heap::priority_queue<std::pair<f32/*score*/, LiteTileHandle>, boost::heap::compare<compareFineNode>> FineNodeHeap;

// Score = (f32)g / 10.0f + h;
class FineNodeList {
public:
    void add(LiteTileHandle handle, ui16 g, f32 h) {
        // TODO: remove G (and maybe parent?) from NodeListElement
        mPriorityHeap.emplace(std::make_pair((f32)g / 10.0f + h, handle));
        mLookup.emplace(handle);
    }

    bool contains(LiteTileHandle handle) {
        return mLookup.find(handle) != mLookup.end();
    }

    void replace(LiteTileHandle handle, ui16 newG, f32 h) {
        // TODO: Figure out to update the priority heap
        assert(false);
    }

    void reserve(size_t count) {
        mPriorityHeap.reserve(count);
        mLookup.reserve(count);
    }

    LiteTileHandle popLowestScoreNode() {
        LiteTileHandle bestHandle = mPriorityHeap.top().second;
        mPriorityHeap.pop();
        mLookup.erase(bestHandle);
        return bestHandle;
    }

    size_t size() const { return mPriorityHeap.size(); }

private:
    FineNodeHeap mPriorityHeap; // For O(log(n)) remove first
    std::unordered_set<LiteTileHandle, LiteTileHandleHash> mLookup; // For o(1) membership test
};

// http://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html
f32 getDiagonalHeuristicAtPosition(const LiteTileHandle& pos, const f32v3& goalPos) {

    const f32v3 worldPos = pos.getWorldPosition();
    //constexpr f32 D = 1;
    constexpr f32 D2 = 1.4f;

    // When D = 1 and D2 = 1, this is called the Chebyshev distance.
    // When D = 1 and D2 = sqrt(2) (1.4), this is called the octile distance.
    // TODO: Truncation error for large integers?? (i dont remember this)
    // TODO: 3D
    f32 dx = abs((f32)worldPos.x - (f32)goalPos.x);
    f32 dy = abs((f32)worldPos.y - (f32)goalPos.y);
    return /*D * */(dx + dy) + (D2 - 2.0f/* * D*/) * vmath::min(dx, dy);
}

f32 getEuclideanHeuristicAtPosition(const f32v3& node, const f32v3& goal) {
    return glm::length(goal - node);
}
//
//constexpr Cartesian8 OPPOSITE_NODE_DIRS[9] = {
//   Cartesian8::NORTH_EAST,   // SOUTH_WEST
//   Cartesian8::NORTH,         // SOUTH
//   Cartesian8::NORTH_WEST,    // SOUTH_EAST
//   Cartesian8::EAST,      // WEST
//   Cartesian8::WEST,       // EAST
//   Cartesian8::SOUTH_EAST, // NORTH_WEST
//   Cartesian8::SOUTH,       // NORTH
//   Cartesian8::SOUTH_WEST,  // NORTH_EAST
//};

constexpr ui16 MOVE_COST_STRAIGHT = 10;
constexpr ui16 MOVE_COST_DIAGONAL = 14;
const ui16 MOVEMENT_COSTS[8] = {
   MOVE_COST_DIAGONAL, // SOUTH_WEST
   MOVE_COST_STRAIGHT, // SOUTH
   MOVE_COST_DIAGONAL, // SOUTH_EAST
   MOVE_COST_STRAIGHT, // WEST
   MOVE_COST_STRAIGHT, // EAST
   MOVE_COST_DIAGONAL, // NORTH_WEST
   MOVE_COST_STRAIGHT, // NORTH
   MOVE_COST_DIAGONAL, // NORTH_EAST
};

const i32v2 NODE_OFFSETS[8] = {
   i32v2(-1, -1), // SOUTH_WEST
   i32v2( 0, -1), // SOUTH
   i32v2( 1, -1), // SOUTH_EAST
   i32v2(-1,  0), // WEST
   i32v2( 1,  0), // EAST
   i32v2(-1,  1), // NORTH_WEST
   i32v2( 0,  1), // NORTH
   i32v2( 1,  1), // NORTH_EAST
};

const i32v2 NODE_CORNER_NEIGHBORS[8] = {
   i32v2(Cartesian8::SOUTH, Cartesian8::WEST),// SOUTH_WEST
   i32v2(0,  0),                         // SOUTH
   i32v2(Cartesian8::SOUTH, Cartesian8::EAST), // SOUTH_EAST
   i32v2(0,  0),                         // WEST
   i32v2(0,  0),                         // EAST
   i32v2(Cartesian8::NORTH, Cartesian8::WEST),  // NORTH_WEST
   i32v2(0,  1),                         // NORTH
   i32v2(Cartesian8::NORTH, Cartesian8::EAST), // NORTH_EAST
};

// TODO: https://gamedev.stackexchange.com/questions/94148/pathfinding-tile-based-navigation-mesh

PathFinder::PathFinder(const World& world) : mWorld(world), mNavWorld(world.getNavWorld()) {

}

struct FineNodeData {
    LiteTileHandle parent;
    ui16 g;
};

// https://github.com/daancode/a-star/blob/master/source/AStar.cpp
bool PathFinder::generateFinePathSynchronous(const TileHandle& start, const TileHandle& goal, OUT NavPath& path) {
    assert(path.numPoints == 0); // Should be uninitialized
    // Only runs on nav thread
    assert(IS_NAV_THREAD());
    // TODO: Profiling
    PreciseTimer timer;
    const WorldGrid& worldGrid = mWorld.getWorldGrid();

    // Make sure we can fit in nodes array
    // We pathfind backwards
    const f32v3 goalWorldPos = start.getWorldPos3D();
    const f32v3 startWorldPos = goal.getWorldPos3D();

    // Instead of an explicit closed list, we use an implicit closed list
    // If a node exists in the gLookup, but not in the openList, then it is in
    // the closed list
    // TODO: Static representation?
    FineNodeList openList;
    std::unordered_map<LiteTileHandle, FineNodeData, LiteTileHandleHash> nodeLookup;
    openList.reserve(MAX_OPEN_LIST_SIZE);
    nodeLookup.reserve(MAX_OPEN_LIST_SIZE);

    // Add start node to the open list (inverted because we pathfind backwards)
    LiteTileHandle startLiteHandle = goal.toLiteTileHandle();
    LiteTileHandle goalLiteHandle = start.toLiteTileHandle();
    openList.add(startLiteHandle, 0, getDiagonalHeuristicAtPosition(startLiteHandle, goalWorldPos));
    nodeLookup.emplace(std::make_pair(startLiteHandle, FineNodeData{ startLiteHandle, 0 }));

    int TOTAL = 0;

    bool foundGoal = false;
    // TODO: Replace open list with boost priority queue like coarse does
    while (openList.size() && openList.size() < MAX_OPEN_LIST_SIZE) {
        ++TOTAL;
        // Pull best node off of the open list (Linear search)
        LiteTileHandle handle = openList.popLowestScoreNode();
        if (handle == goalLiteHandle) {
            foundGoal = true;
            break;
        }

        const ui16 g = nodeLookup.find(handle)->second.g;
        TileContainer* container = handle.getTileContainer();
        const i32v3& dims = container->getDims();
        const TileFineNavData& fineNavData = container->getFineNavData()[handle.index];
        // Add current node to implicit closed list

        // Precompute collision weights and points for neighbors
        for (int dir = (int)Cartesian8::SOUTH_WEST; dir <= (int)Cartesian8::NORTH_EAST; ++dir) {
            if (!fineNavData.canAccessDirection(Cartesian8(dir))) {
                continue;
            }

            f32 pathWeight;
            const i32v2& adjOffset = NODE_OFFSETS[dir];
            
            LiteTileHandle adjHandle;
            Cartesian cartesian4 = CARTESIAN8_TO_CARTESIAN[dir];
            const TileFineNavEdgeType edgeType = fineNavData.getEdgeType(cartesian4);
            bool isExternal = (cartesian4 != Cartesian::NONE) && (edgeType == TileFineNavEdgeType::EXTERIOR);
            if (isExternal/* || adjPos.x < 0 || adjPos.y < 0 || adjPos.x >= dims.x || adjPos.y >= dims.y || !container->isTileOwned(adjIndex)*/) {
                // External edge
                const f32 zPos = handle.toTileHandle().tile->getGroundZOffsetThreadSafe();
                const i32v3 containerOffset = container->getTileXYZOffsetWithZScale(handle.index);
                const i32v3 offset(containerOffset.x + adjOffset.x, containerOffset.y + adjOffset.y, glm::round(containerOffset.z + zPos));
                const TileHandle externalHandle = mWorld.getTileHandleAtWorldPosThreadSafe(offset + container->getWorldPos3D());
                if (!externalHandle.isValid()) {
                    continue;
                }
                adjHandle = externalHandle.toLiteTileHandle();
            }
            else {
                // Internal edge
                const i32v3 containerOffset = container->getTileXYZOffset(handle.index);
                i32v3 adjPos(containerOffset.x + adjOffset.x, containerOffset.y + adjOffset.y, containerOffset.z);
                if (edgeType == TileFineNavEdgeType::DOWN) {
                    --adjPos.z;
                }
                else if (edgeType == TileFineNavEdgeType::UP) {
                    ++adjPos.z;
                }
                TileIndex adjIndex = container->getTileIndexFromXYZOffset(adjPos);
                adjHandle = LiteTileHandle(container->getId(), adjIndex);
            }
            // Get path weight
            const TileFineNavData& adjNavData = adjHandle.getTileContainer()->getFineNavData()[adjHandle.index];
            pathWeight = adjNavData.pathWeight / 255.0f;

            // Compute cost to the adj node
            const ui16 newG = g + MOVEMENT_COSTS[dir] / pathWeight;

            // 0 weight means we cannot path
            if (pathWeight == 0.0f) {
                continue;
            }

            // Get our previous G if it exists
            constexpr ui16 INVALID_G = UINT16_MAX;
            ui16 prevG = INVALID_G;
            auto&& it = nodeLookup.find(adjHandle);
            if (it != nodeLookup.end()) {
                prevG = it->second.g;
            }

            // We dont do anything if this is a worse path than what we had before
            if (newG < prevG) {
                if (openList.contains(adjHandle)) {
                    // New node is better than current openlist node
                    std::cout << "Detected open list node with better priority. TODO: Implement increase priority and benchmark\n";
                    //openList.replace(adjHandle, newG, getDiagonalHeuristicAtPosition(adjHandle, goalWorldPos));
                    //it->second.g = newG;
                    //it->second.parent = handle;
                    //assert(handle.isValid());
                }
                else {
                    // This is an unvisited node OR
                    // this node was already processed but with a worse score
                    openList.add(adjHandle, newG, getDiagonalHeuristicAtPosition(adjHandle, goalWorldPos));
                    // Update or insert into gLookup
                    if (it != nodeLookup.end()) {
                        it->second.g = newG;
                        it->second.parent = handle;
                        assert(handle.isValid());
                    }
                    else {
                        nodeLookup.emplace(std::make_pair(adjHandle, FineNodeData{ handle, newG }));
                        assert(handle.isValid());
                    }
                }
            }
        }
    }

    if (!foundGoal) {
        std::cout << "Failed to find path in " << timer.stop() << " ms\n";
        path.finishedGenerating.store(true);
        return false;
    }

    // Generate the path by reverse iterating from the goal
    ui32 pathSize = 0;
    {
        LiteTileHandle handle = goalLiteHandle;

        // Find out the path size and cache the tile handles
        while (handle != startLiteHandle && pathSize < PATH_POINT_BUFFER_SIZE) {
            sPathPointBuffer[pathSize] = handle;
            assert(sPathPointBuffer[pathSize].isValid());
            auto&& it = nodeLookup.find(handle);
            assert(it != nodeLookup.end());
            handle = it->second.parent;
            ++pathSize;
        }
    }

    if (pathSize == PATH_POINT_BUFFER_SIZE) {
        // Warning! Path is too long!
        std::cout << "Failed to find path in " << timer.stop() << " ms because result path is too long\n";
        path.finishedGenerating.store(true);
        return false;
    }

    path.allocatePath(pathSize);
    // Copy the path
    memcpy(path.points, sPathPointBuffer, pathSize * sizeof(LiteTileHandle));

    std::cout << "Generated path in " << timer.stop() << " ms with " << TOTAL << " nodes checked\n";
    path.finishedGenerating.store(true);
    return true;
}

bool PathFinder::generateCoarsePathSynchronous(const TileHandle& start, const TileHandle& goal, OUT NavPath& path)
{
    assert(IS_NAV_THREAD());
    assert(path.numPoints == 0); // Should be uninitialized

    if (start.tile->getNavNodeIndex() == INVALID_NAV_NODE_INDEX || goal.tile->getNavNodeIndex() == INVALID_NAV_NODE_INDEX) {
        pError("Error: Failed to find coarse path due to invalid start\n");
        path.finishedGenerating.store(true);
        return false;
    }

    PreciseTimer timer;
    
    mOpenList.clear();
    mOpenList.reserve(MAXIMUM_COARSE_NODES);
    
    const WorldGrid& worldGrid = mWorld.getWorldGrid();
    const TileContainer* startContainer = start.container;
   
    // We pathfind backwards
    const CoarseNavGraph& startNavGraph = mNavWorld.getCoarseNavGraph(goal.container->getId());
    const CoarseNavNode* startNode = &startNavGraph.getNode(goal.tile->getNavNodeIndex());
    const CoarseNavNode* endNode = mNavWorld.getCoarseNavNode(start.container->getId(), start.tile->getNavNodeIndex());

    // Case where we are in the same node, just return the goal
    if (startNode == endNode) {
        path.allocatePath(1);
        path.points[0] = goal.toLiteTileHandle();
        path.finishedGenerating.store(true);
        return true;
    }

    // Inverted since we pathfind backwards
    const f32v3 startWorldPos = goal.getWorldPos3D();
    const f32v3 goalWorldPos = start.getWorldPos3D();

    // A* pathfind through the coarse graph
    // TODO: non arbitrary reserve (Is this fixed?)
    //sCoarseClosedList.insert(nullptr); // So we dont need explicit null check later
    mCoarseClosedList.clear();
    mCoarseClosedList.reserve(MAXIMUM_COARSE_NODES);

    mTotalAstarNodes = 0;
    CoarseAstarNodeID id;
    bool foundGoal = false;

    // TODO: Race conditions with the navgraph generator thread?
    // Add all first edges to the open and closed lists
    //sCoarseClosedList.insert(startNode);
    // 
    // TODO: According to the algorithm this should happen at the end of while loop
    startNode->isClosed = true;
    mCoarseClosedList.push_back(startNode);
    coarseAstarEdgePropagate(startNode, goal, startNavGraph, goalWorldPos, INVALID_COARSE_NODE_PARENT, 0.0f);
    // Do the A*
    while (mOpenList.size() && mTotalAstarNodes < MAXIMUM_COARSE_NODES - 256) {
        const auto& topNode = mOpenList.top();
        id = topNode.second;
        CoarseAStarNode& astarNode = sCoarseAstarNodes[id];
        
        mOpenList.pop();

        TileHandle handle = astarNode.tileHandle.toTileHandle();
        ui16 navNodeIndex = handle.container->getTileAt(handle.tileIndex).getNavNodeIndex();
        const CoarseNavGraph& navGraph = mNavWorld.getCoarseNavGraph(astarNode.tileHandle.containerId);
        const CoarseNavNode* navNode = &navGraph.getNode(navNodeIndex);
        if (navNode == endNode) {
            foundGoal = true;
            break;
        }

        coarseAstarEdgePropagate(navNode, handle, navGraph, goalWorldPos, id, astarNode.g);
    }

    for (auto&& node : mCoarseClosedList) {
        node->isClosed = false;
    }

    // TODO: Threadsafety
    /*if (sDebugOptions.mShowPaths) {
        f32v3 start3d = helperGet3DPoint(worldGrid, f32v2(start));
        DebugRenderer::drawFilledQuad(start3d, f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.6f), DEBUG_DURATION, 0);
        f32v3 goal3d = helperGet3DPoint(worldGrid, f32v2(goal));
        DebugRenderer::drawFilledQuad(goal3d, f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 0.6f), DEBUG_DURATION, 0);
    }*/

    if (!foundGoal) {
        std::cout << "Coarse path failed in " << timer.stop() << "ms with " << mTotalAstarNodes << " total nodes checked\n";
        path.finishedGenerating.store(true);
        return false;
    }

    ui32 pathSize = 0;
    {
        sPathPointBuffer[pathSize++] = start.toLiteTileHandle();
        CoarseAstarNodeID parentId = id;
        // Find out the path size and cache the points
        while (parentId != INVALID_COARSE_NODE_PARENT) {
            CoarseAStarNode& astarNode = sCoarseAstarNodes[parentId];
            sPathPointBuffer[pathSize++] = astarNode.tileHandle;
            parentId = astarNode.parentIndex;
        }
    }

    // Append goal if it isnt at the endpoint already
    LiteTileHandle goalLiteHandle = goal.toLiteTileHandle();
    if (sPathPointBuffer[pathSize - 1] != goalLiteHandle) {
        sPathPointBuffer[pathSize++] = goalLiteHandle;
    }

    // TODO: Path memory pool
    path.allocatePath(pathSize);
    // Copy the path
    memcpy(path.points, sPathPointBuffer, pathSize * sizeof(LiteTileHandle));


    if (sDebugOptions.mShowPaths) {
        for (ui32 i = 1; i < path.numPoints; ++i) {
            assert(path.points[i - 1].isValid());
            const f32v3 a = path.points[i - 1].getWorldPosition();
            assert(path.points[i].isValid());
            const f32v3 b = path.points[i].getWorldPosition();
            DebugRenderer::drawLineBetweenPointsThreadSafe(a, b, color4(1.0f, 1.0f, 0.0f, 0.6f), DEBUG_DURATION);
        }
    }

    std::cout << "Coarse path found in " << timer.stop() << "ms with " << mTotalAstarNodes << " total nodes checked\n";
    path.finishedGenerating.store(true);
    return true;
}

class EdgeNodeHash {
public:
    size_t operator()(const std::pair<TileContainerID, ui32>& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.first);
        boost::hash_combine(seed, v.second);
        return seed;
    }
};

void PathFinder::coarseAstarEdgePropagate(const CoarseNavNode* navNode, const TileHandle& tileHandle, const CoarseNavGraph& navGraph, const f32v3& goalPos, CoarseAstarNodeID parentId, f32 prevG) {
    const TileContainer* container = tileHandle.container;
    const i32v3& containerDims = container->getDims();
    const TileIndex internalIndexOffsetsCartesian[4] = {
        -containerDims.x, // South
        -1,               // West
         1,               // East
        containerDims.x   // North
    };

    const i32v3 tilePos = tileHandle.getWorldPos3D();
    // Iterate all edges
    for (ui16 i = 0; i < navNode->edgeCount; ++i) {
        CoarseNavNodeEdge& edge = navNode->edges[i];
        if (edge.isExternalEdge()) {
            // With external edges we have to look up the adjacent nav nodes
            std::unordered_map<std::pair<TileContainerID, ui32 /*navNode*/>, TileIndex, EdgeNodeHash> edgeNodes;
            i32v3 edgeStartPosWorld = container->getWorldPos3D() + container->getTileXYZOffsetWithZScale(edge.startPos);
            for (int i = 0; i < (int)edge.edgeLength; ++i) {
                const i32v3& edgeDir = CARTESIAN_EDGE_DIRS_ABS_3D[e_cast(edge.dir)];
                const i32v3 edgePosWorld = edgeStartPosWorld + edgeDir * i;
                const TileIndex nextIndex = edge.startPos + (edgeDir.x + edgeDir.y * container->getDims().y) * i;
                const Tile& innerTile = container->getTileAt(nextIndex);
                i32v3 worldPosOuter = edgePosWorld + CARTESIAN_NORMALS_3D[e_cast(edge.dir)];
                worldPosOuter.z = glm::round(worldPosOuter.z + innerTile.getGroundZOffsetThreadSafe());
                TileHandle outerHandle = mWorld.getTileHandleAtWorldPosThreadSafe(worldPosOuter);
                if (!outerHandle.isValid()) {
                    continue;
                }
                // This should be very rare and is a failure case for this edge, TODO: debug log it or something?
                assert(outerHandle.container != container);
                // TODO: This always picks last node
                edgeNodes[std::make_pair(outerHandle.container->getId(), outerHandle.tile->getNavNodeIndex())] = outerHandle.tileIndex;
            }
            for (auto&& it : edgeNodes) {
                //TileContainer* adjContainer = TileContainerRepository::getTileContainer(it.first);
                const TileContainerID nextContainerId = it.first.first;
                const CoarseNavGraph* adjNavGraph = mNavWorld.tryGetCoarseNavGraph(nextContainerId);
                if (!adjNavGraph) {
                    continue;
                }
                const CoarseNavNode& nextNode = adjNavGraph->getNode(it.first.second);
                if (nextNode.isClosed) {
                    continue;
                }

                // Add position and node to the lists as below
                nextNode.isClosed = true;
                mCoarseClosedList.push_back(&nextNode);

                const CoarseAstarNodeID newId = mTotalAstarNodes++;
                CoarseAStarNode& newAstarNode = sCoarseAstarNodes[newId];
                newAstarNode.tileHandle = LiteTileHandle(nextContainerId, it.second);
                const i32v3 nextPos = newAstarNode.tileHandle.getWorldPosition();
                newAstarNode.g = prevG + glm::length(f32v3(nextPos - tilePos));
                newAstarNode.h = getEuclideanHeuristicAtPosition(nextPos, goalPos);
                if (sDebugOptions.mShowPaths) {
                    DebugRenderer::drawLineBetweenPointsThreadSafe(nextPos, tilePos, color4(((int)newAstarNode.g % 255) / 255.0f, ((int)newAstarNode.h % 255) / 255.0f, 0.0f, 0.5f), DEBUG_DURATION);
                }
                newAstarNode.parentIndex = parentId;
                mOpenList.push(std::make_pair(newAstarNode.getScore(), newId));
            }
        }
        else {
            // Internal edges will store the adjacent nav nodes and share tileContainer
            const CoarseNavNode& nextNode = navGraph.getNode(edge.adjacentNodeIndex);
            if (nextNode.isClosed) {
                continue;
            }

            // Right now we are deliberately not allowing path to be re-updated
            // because its a lot cheaper
            nextNode.isClosed = true;
            mCoarseClosedList.push_back(&nextNode);
            
            const Cartesian edgeWalkDir = CARTESIAN_COARSE_EDGE_WALK_CARTESIAN[e_cast(edge.dir)];
            const TileIndex midPoint = edge.startPos + internalIndexOffsetsCartesian[e_cast(edgeWalkDir)] * (edge.edgeLength / 2);
            TileIndex nextTileIndex = midPoint + internalIndexOffsetsCartesian[e_cast(edge.dir)];
            if (edge.edgeType == TileCoarseNavEdgeType::DOWN) {
                nextTileIndex -= container->getDims().x * container->getDims().y;
            }
            else if (edge.edgeType == TileCoarseNavEdgeType::UP) {
                nextTileIndex += container->getDims().x * container->getDims().y;
            }
            const CoarseAstarNodeID newId = mTotalAstarNodes++;
            CoarseAStarNode& newAstarNode = sCoarseAstarNodes[newId];
            newAstarNode.tileHandle = LiteTileHandle(tileHandle.container->getId(), nextTileIndex);
            const i32v3 nextPos = newAstarNode.tileHandle.getWorldPosition();
            newAstarNode.g = prevG + glm::length(f32v3(nextPos - tilePos));
            newAstarNode.h = getEuclideanHeuristicAtPosition(nextPos, goalPos);
            if (sDebugOptions.mShowPaths) {
                DebugRenderer::drawLineBetweenPointsThreadSafe(nextPos, tilePos, color4(((int)newAstarNode.g % 255) / 255.0f, ((int)newAstarNode.h % 255) / 255.0f, 1.0f, 0.5f), DEBUG_DURATION);
            }
            newAstarNode.parentIndex = parentId;
            mOpenList.push(std::make_pair(newAstarNode.getScore(), newId));
        }
    }
}
