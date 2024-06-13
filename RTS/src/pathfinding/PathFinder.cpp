#include "stdafx.h"
#include "PathFinder.h"

#include "world/World.h"
#include "resources/TileRepository.h"
#include "NavPath.h"
#include "world/IHeightmapGrid.h"

#include "pathfinding/NavWorld.h"

#include "options/DebugOptions.h"

#include "debugging/DebugRenderer.h"

#include "building/building.h"

constexpr Cartesian CARTESIAN_COARSE_EDGE_WALK_CARTESIAN[4] = {
    Cartesian::EAST, //South
    Cartesian::NORTH, //West
    Cartesian::NORTH, //East
    Cartesian::EAST, //North
};

constexpr ui32 MAX_PATH_LENGTH = 128; //255;
constexpr ui8 INVALID_PARENT = 0;

constexpr ui32 MAX_FINE_OPEN_LIST_SIZE = 512;

constexpr ui32 DEBUG_DURATION = 10;

struct CoarseAStarNode {
    LiteTileHandle tileHandle;
    f32 h; // Heuristic distance to target
    f32 g; // Movement cost to the node
    ui16 parentIndex;

    f32 getScore() const { return g + h; }
};

constexpr CoarseAstarNodeID MAXIMUM_COARSE_NODES = 8196;
thread_local CoarseAStarNode sCoarseAstarNodes[MAXIMUM_COARSE_NODES];   

// No allocations
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
    void addReplace(LiteTileHandle handle, ui16 g, f32 h) {
        // TODO: This isn't a true replace it is a duplicate
        mPriorityHeap.emplace(std::make_pair((f32)g / 10.0f + h, handle));
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
f32 getDiagonalHeuristicAtPosition(const f32v3& pos, const f32v3& goalPos) {

    //constexpr f32 D = 1;
    constexpr f32 D2 = 1.4f;

    // When D = 1 and D2 = 1, this is called the Chebyshev distance.
    // When D = 1 and D2 = sqrt(2) (1.4), this is called the octile distance.
    // TODO: Truncation error for large integers?? (i dont remember this)
    // TODO: 3D
    f32 dx = abs((f32)pos.x - (f32)goalPos.x);
    f32 dy = abs((f32)pos.y - (f32)goalPos.y);
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
   i32v2(Cartesian8::NORTH, Cartesian8::EAST), // NORTH_EASTf
};

// TODO: https://gamedev.stackexchange.com/questions/94148/pathfinding-tile-based-navigation-mesh

PathFinder::PathFinder(const NavWorld& navWorld) : mNavWorld(navWorld) {

}

struct FineNodeData {
    LiteTileHandle parent;
    ui16 g;
    //ui16 whatever;
};

// https://github.com/daancode/a-star/blob/master/source/AStar.cpp
bool PathFinder::generateFinePathSynchronous(const f32v3 start, const f32v3 goal, f32 targetRadius, OUT NavPath& path) {
    PROFILE_FUNCTION();
    assert(path.numPoints == 0); // Should be uninitialized
    // Only runs on nav thread
    ASSERT_NAV_THREAD();
    // TODO: Profiling
    PreciseTimer timer;
    const IHeightmapGrid& heightGrid = mNavWorld.getWorld().getHeightmapGrid();

    // We pathfind backwards
    const ContainerNavData* startNavData = nullptr;
    const ContainerNavData* goalNavData = nullptr;
    LiteTileHandle startLiteHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(goal, &startNavData);
    LiteTileHandle goalHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(start, &goalNavData);

    if (!startNavData) {
        LOG_WARN("Failed to find fine path due to invalid start");
        path.finishedGenerating.store(true);
        return false;
    }

    const f32v3 startWorldPos = startNavData->getTileWorldPos(startLiteHandle.index);
    // Backwards pathfind
    f32v3 goalWorldPos = start;
    if (goalNavData) {
        goalWorldPos = goalNavData->getTileWorldPos(goalHandle.index);
    }

    // Instead of an explicit closed list, we use an implicit closed list
    // If a node exists in the nodeLookup, but not in the openList, then it is in
    // the closed list
    // TODO: Static representation?
    FineNodeList openList;
    std::unordered_map<LiteTileHandle, FineNodeData, LiteTileHandleHash> nodeLookup;
    openList.reserve(MAX_FINE_OPEN_LIST_SIZE);
    nodeLookup.reserve(MAX_FINE_OPEN_LIST_SIZE);

    // Add start node to the open list
    openList.add(startLiteHandle, 0, getDiagonalHeuristicAtPosition(startWorldPos, goalWorldPos));
    nodeLookup.emplace(std::make_pair(startLiteHandle, FineNodeData{ LiteTileHandle() /*parent*/, 0 }));

    int TOTAL = 0;

    bool foundGoal = false;
    LiteTileHandle handle;
    while (openList.size() && openList.size() < MAX_FINE_OPEN_LIST_SIZE) {
        ++TOTAL;
        // Pull best node off of the open list
        handle = openList.popLowestScoreNode();
        const ContainerNavData& containerNavData = mNavWorld.getNavDataForContainer(handle.containerId);
        const f32v3 worldPos = containerNavData.getTileWorldPos(handle.index);

        constexpr f32 SUCCESS_DISTANCE_SQ = SQ(1.5f); // Tweak this as needed
        if (glm::length2(worldPos - goalWorldPos) < SUCCESS_DISTANCE_SQ) {
            foundGoal = true;
            break;
        }

        // Score and nav data lookup
        const ui16 g = nodeLookup.find(handle)->second.g; // TODO: Can we potentially stop this lookup by storing it in the openList? Profile
        const TileFineNavData fineNavData = containerNavData.fineNavGraph[handle.index];

        // Debug render
        if (sDebugOptions.mShowPaths) {
            const ui8 r = (ui8)(g % 256);
            DebugRenderer::drawWireQuadThreadSafe(containerNavData.getTileWorldPos(handle.index), f32v2(1.0f), color4(r, 0ui8, (ui8)(255ui8 - r), 128ui8), DEBUG_DURATION);
        }

        // Precompute collision weights and points for neighbors
        for (int dir = (int)Cartesian8::SOUTH_WEST; dir <= (int)Cartesian8::NORTH_EAST; ++dir) {
            Cartesian8 cartesianDir = Cartesian8(dir);
            if (!fineNavData.canAccessDirection(cartesianDir)) {
                continue;
            }

            f32 pathWeight = 1.0f;
            const i32v2& adjOffset = NODE_OFFSETS[dir];

            LiteTileHandle adjHandle;
            TileFineNavData adjFineNavData;

            Cartesian cartesian4 = CARTESIAN8_TO_CARTESIAN[dir];
            const TileFineNavEdgeType edgeType = fineNavData.getEdgeType(cartesian4);
            bool isExternal = (edgeType == TileFineNavEdgeType::EXTERIOR);
            if (isExternal) {
                assert(cartesian4 != Cartesian::NONE);
                // External edge
                const i32v3 containerOffset = TileSpatialGrid::getTileXYZOffsetWithZScale(handle.index, containerNavData.containerDims, containerNavData.floorHeight);
                const i32v3 offset(containerOffset.x + adjOffset.x, containerOffset.y + adjOffset.y, glm::round(containerOffset.z + fineNavData.zPositionOffsetFromFloor));

                const ContainerNavData* outNavData = nullptr;
                adjHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(offset + containerNavData.worldPos, &outNavData);
                // Can occur when we have ledges internally
                // TODO: Evaluate if we can mark these ledges as internal
                if (adjHandle.containerId == handle.containerId) [[unlikely]] {
                    continue;
                }
                if (outNavData) {
                    assert(outNavData->containerId != handle.containerId);
                    adjFineNavData = outNavData->fineNavGraph[adjHandle.index];
                }
                else {
                    // No valid data
                    continue;
                }
                // Check if we can enter the adjacent file tile
                // TODO: Handle dropping down
                // Are there issues with dropping down since we pathfind in reverse?
                if (adjFineNavData.canAccessDirection(CARTESIAN8_OPPOSITES[e_cast(cartesianDir)])) {
                    const int myFloorIndex = containerOffset.z;
                    const f32 myZPos = fineNavData.zPositionOffsetFromFloor + myFloorIndex * containerNavData.floorHeight + containerNavData.worldPos.z;
                    const int adjFloorIndex = adjHandle.index / (outNavData->containerDims.x * outNavData->containerDims.y);
                    const f32 zPosAdjacent = adjFineNavData.zPositionOffsetFromFloor + adjFloorIndex * outNavData->floorHeight + outNavData->worldPos.z;
                    // We pathfind in reverse
                    constexpr f32 MAX_DROP_HEIGHT = 5.0f;
                    constexpr f32 MAX_CLIMB_HEIGHT = 1.5f;
                    constexpr f32 DROP_PATH_WEIGHT = 0.75f;
                    constexpr f32 CLIMB_PATH_WEIGHT = 0.3f;
                    if (zPosAdjacent > myZPos && zPosAdjacent - myZPos < MAX_DROP_HEIGHT) {
                        // Drop down
                        adjFineNavData = outNavData->fineNavGraph[adjHandle.index];
                        pathWeight = DROP_PATH_WEIGHT;
                        assert(adjFineNavData.isOwned);

                    }
                    else if (myZPos > zPosAdjacent && myZPos - zPosAdjacent < MAX_CLIMB_HEIGHT) {
                        // Climb up
                        adjFineNavData = outNavData->fineNavGraph[adjHandle.index];
                        pathWeight = CLIMB_PATH_WEIGHT;
                        assert(adjFineNavData.isOwned);
                    }
                    else {
                        continue;
                    }
                }
                else {
                    continue;
                }
            }
            else {
                // Internal edge
                const i32v3 containerOffset = TileSpatialGrid::getTileXYZOffset(handle.index, containerNavData.containerDims);
                i32v3 adjPos(containerOffset.x + adjOffset.x, containerOffset.y + adjOffset.y, containerOffset.z);
                if (edgeType == TileFineNavEdgeType::DOWN) {
                    --adjPos.z;
                }
                else if (edgeType == TileFineNavEdgeType::UP) {
                    ++adjPos.z;
                }
                TileIndex adjIndex = TileSpatialGrid::getTileIndexFromXYZOffset(adjPos, containerNavData.containerDims);
                adjHandle = LiteTileHandle(handle.containerId, adjIndex);
                adjFineNavData = containerNavData.fineNavGraph[adjHandle.index];
                assert(adjFineNavData.isOwned);
            }
            // Get path weight
            pathWeight *= ((f32)adjFineNavData.pathWeight / 255.0f);

            // Compute cost to the adj node
            const ui16 newG = (ui16)(g + MOVEMENT_COSTS[dir] / pathWeight);

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
                const f32v3 adjPos = mNavWorld.getNavDataForContainer(adjHandle.containerId).getTileWorldPos(adjHandle.index);
                if (openList.contains(adjHandle)) {
                    // TODO: Based on https://gist.github.com/ryancollingwood/32446307e976a11a1185a5394d6657bc, we are not removing the existing node
                    // just adding a new node. Instead we could remove the existing one with increase_priority
                    
                    // New node is better than current openlist node
                    // TODO: Will we cause a problem if we process the same node twice?
                    //LOG_DEBUG("Detected open list node with better priority. TODO: Implement increase priority and benchmark");
                    openList.addReplace(adjHandle, newG, getDiagonalHeuristicAtPosition(adjPos, goalWorldPos));
                    // Store new score in the lookup
                    it->second.g = newG;
                    it->second.parent = handle;
                }
                else {
                    // If we are in the node lookup, we are closed
                    if (it == nodeLookup.end()) {
                        openList.add(adjHandle, newG, getDiagonalHeuristicAtPosition(adjPos, goalWorldPos));
                        nodeLookup.emplace(std::make_pair(adjHandle, FineNodeData{ handle, newG }));
                        assert(handle.isValid());
                    }
                }
            }
        }
    }

    if (!foundGoal) {
        LOG_WARN("Failed to find fine path in {} ms with {} total nodes checked. OpenListSize: {}", timer.stop(), TOTAL, openList.size());
        if (sDebugOptions.mShowPaths) {
            LiteTileHandle handleIt = handle;

            // Draw failed path for debug vis
            ui32 c = 0;
            f32v3 zero = f32v3(0.0f);
            while (handleIt != startLiteHandle && c < PATH_POINT_BUFFER_SIZE) {
                auto&& it = nodeLookup.find(handleIt);
                assert(it != nodeLookup.end());
                handleIt = it->second.parent;
                const ContainerNavData& navData = mNavWorld.getNavDataForContainer(handleIt.containerId);
                assert(handleIt.index < navData.fineNavGraph.size());
                const f32v3 pos = navData.getTileWorldPos(handleIt.index);
                if (c == 0) {
                    zero = pos;
                    DebugRenderer::drawWireQuadThreadSafe(pos + f32v3(0.0f, 0.0f, 1.0f), f32v2(1.0f), COLOR_CYAN, DEBUG_DURATION * 64);
                }
                else {
                    DebugRenderer::drawWireQuadThreadSafe(pos, f32v2(1.0f), COLOR_MAGENTA, DEBUG_DURATION);
                }
                ++c;
            }
            DebugRenderer::drawWireQuadThreadSafe(start + f32v3(0.0f, 0.0f, 1.0f), f32v2(1.0f), COLOR_GREEN, DEBUG_DURATION * 64);
            if (c != 0) {
                DebugRenderer::drawLineBetweenPointsThreadSafe(zero, start, color::Red, DEBUG_DURATION * 64);
            }
            else {
                DebugRenderer::drawFilledQuadThreadSafe(start + f32v3(0.0f, 0.0f, 1.0f), f32v2(1.1f), COLOR_YELLOW, DEBUG_DURATION * 64);
            }
                 
        }
        path.finishedGenerating.store(true);
        return false;
    }

    // Generate the path by reverse iterating from the last point
    ui32 pathSize = 0;
    {
        LiteTileHandle handleIt = handle;

        auto walkable = [](LiteTileHandle src, LiteTileHandle dst) -> bool {
            return true;
        };

        // Find out the path size and cache the tile handles
        sPathPointBuffer[pathSize++] = handleIt;
        while (handleIt != startLiteHandle && pathSize < PATH_POINT_BUFFER_SIZE) {
            assert(handleIt.isValid());
            
            auto&& it = nodeLookup.find(handleIt);
            assert(it != nodeLookup.end());

            // Skip over walkable nodes to reduce path size
            LiteTileHandle* nextNode = &it->second.parent;
            while (*nextNode != startLiteHandle) {
                assert(nextNode->isValid());
                if (walkable(handleIt, *nextNode)) {
                    auto&& itNext = nodeLookup.find(*nextNode);
                    assert(itNext != nodeLookup.end());
                    nextNode = &itNext->second.parent;
                }
                else {
                    break;
                }
            }

            handleIt = *nextNode;
            sPathPointBuffer[pathSize++] = handleIt;
        }

    }

    if (pathSize == PATH_POINT_BUFFER_SIZE) {
        // Warning! Path is too long!
        LOG_WARN("Failed to find fine path in {} ms with {} total nodes checked because result path is too long", timer.stop(), TOTAL);
        path.finishedGenerating.store(true);
        return false;
    }

    // If we have a null path, make sure we append a single node at least
    if (pathSize == 0) {
        sPathPointBuffer[pathSize++] = startLiteHandle;
    }

    path.allocatePath(pathSize);
    // Copy the path
    for (int i = 0; i < (int)pathSize; ++i) {
        LiteTileHandle& handle = sPathPointBuffer[i];
        path.points[i] = f32v3(mNavWorld.getNavDataForContainer(handle.containerId).getTileWorldPos(handle.index)) + f32v3(0.5f, 0.5f, 0.0f);
    }

    LOG_TRACE("Generated fine path in {} ms with {} total nodes checked", timer.stop(), TOTAL);
    path.targetPosition = goal;
    path.finishedGenerating.store(true);
    return true;
}

bool PathFinder::generateCoarsePathSynchronous(const f32v3 start, const f32v3 goal, f32 targetRadius, OUT NavPath& path) {
    PROFILE_FUNCTION();
    ASSERT_NAV_THREAD();
    assert(path.numPoints == 0); // Should be uninitialized
    const f32 targetRadiusSQ = SQ(targetRadius);

    // We pathfind backwards so swap start and goal
    const ContainerNavData* startNavData = nullptr;
    const ContainerNavData* goalNavData = nullptr;
    LiteTileHandle startHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(glm::floor(goal), &startNavData);
    LiteTileHandle goalHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(glm::floor(start), &goalNavData);

    LiteTileHandle FIRST_START_HANDLE = startHandle;

    if (!goalNavData) {
        LOG_WARN("Failed to find coarse path due to invalid start nav data");
        path.finishedGenerating.store(true);
        return false;
    }

    const CoarseNavNodeIndex goalNavNodeIndex = goalNavData->coarseNavGraph.tileCoarseNavIndices[goalHandle.index];

    if (goalNavNodeIndex == INVALID_NAV_NODE_INDEX) {
        // Reversed on purpose
        LOG_WARN("Failed to find coarse path due to invalid start navnode index at tile handle {}", goalHandle.index);
        const f32v3 pos = goalNavData->getTileWorldPos(goalHandle.index);
        DebugRenderer::drawFilledQuadThreadSafe(pos, f32v2(1.0f), color::Red, DEBUG_DURATION * 64);
        path.finishedGenerating.store(true);
        return false;
    }

    // If our target is invalid, it means we are trying to navigate to an unloaded chunk, which is valid!
    // We will step from the target towards the start (goal, in reverse pathfinding) until we hit a valid handle, and
    // we will use that as the new target. Binary search makes this fast, usually no more than 10 or 11 iteration
    if (!startNavData) {
       ChunkCoord goalChunkCoord = ChunkCoord::fromTilePos(goal);
       path.simChunkEndPoint = goalChunkCoord.toGridIDType(mNavWorld.getWidthChunks());

       f32v3 totalSpan = start - goal;
       f32 spanLength = glm::length(totalSpan);
       f32v3 direction = totalSpan / spanLength;

       spanLength *= 0.5f;
       f32v3 currentPos = goal + direction * spanLength;
       constexpr f32 MIN_STEP = 1.0f;
       do {
           // Binary search along the total span until we hit a valid handle and
           // the step distance is < min step
           const ContainerNavData* navData = nullptr;
           LiteTileHandle handle = mNavWorld.getTileHandleAndNavDataAtWorldPos(currentPos, &navData);
           spanLength *= 0.5f;
           if (navData) {
               // Cache closest valid nav to the edge
               startHandle = handle;
               startNavData = navData;

               // Go back
               currentPos -= direction * spanLength;
           }
           else {
               // Cache closest sim chunk ID to the edge
               ChunkCoord chunkCoord = ChunkCoord::fromTilePos(currentPos);
               path.simChunkEndPoint = goalChunkCoord.toGridIDType(mNavWorld.getWidthChunks());

               currentPos += direction * spanLength;
           }
       } while (spanLength >= MIN_STEP);
    }

    if (!startNavData) {
        LOG_WARN("Failed to find coarse path due to invalid edge search");
        path.finishedGenerating.store(true);
        return false;
    }

    CoarseNavNodeIndex startNavNodeIndex = startNavData->coarseNavGraph.tileCoarseNavIndices[startHandle.index];
   

    if (sDebugOptions.mShowPaths) {
        DebugRenderer::drawWireQuadThreadSafe(goal, f32v2(1.0f), COLOR_CYAN, DEBUG_DURATION);
    }
    // Since we reverse pathfind, this is the target, and may be inside something. Need to BFS spread to the closest valid node
    if (startNavNodeIndex == INVALID_NAV_NODE_INDEX) {
        constexpr i32 MAX_OPEN_SIZE = 1024;
        f32v3 openList[MAX_OPEN_SIZE];
        std::unordered_set<f32v3, f32v3hash> closedList;
        closedList.reserve(MAX_OPEN_SIZE);
        i32 openListFront = 0;
        i32 openListBack = 1;
        openList[0] = goal;
        closedList.insert(goal);

        // Sort cartesians based on goal direction so we
        // tend to select closest first
        std::array<Cartesian, 4> cartesians = { Cartesian::SOUTH, Cartesian::WEST, Cartesian::EAST, Cartesian::NORTH };
        const f32v2 goalDir = glm::normalize(start - goal);
        std::sort(cartesians.begin(), cartesians.end(), [goalDir](Cartesian a, Cartesian b) {

            const f32v2 dirA(CARTESIAN_NORMALS_2D[e_cast(a)]);
            const f32v2 dirB(CARTESIAN_NORMALS_2D[e_cast(b)]);

            f32 dotA = glm::dot(dirA, goalDir);
            f32 dotB = glm::dot(dirB, goalDir);

            return dotA > dotB;
        });

        // Breadth first search
        while (openListFront < openListBack && openListBack < MAX_OPEN_SIZE - 4) {
            const f32v3 currentPos = openList[openListFront++];
            startHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(glm::floor(currentPos), &startNavData);
            if (!startHandle.isValid()) {
                DebugRenderer::drawFilledQuadThreadSafe(currentPos, f32v2(1.1f), COLOR_CYAN, DEBUG_DURATION * 64);
                break;
            }
            startNavNodeIndex = startNavData->coarseNavGraph.tileCoarseNavIndices[startHandle.index];
            if (startNavData && startNavNodeIndex != INVALID_NAV_NODE_INDEX) {
                const f32v3 truePos = startNavData->getTileWorldPos(startHandle.index);
                if (glm::length2(truePos - goal) < targetRadiusSQ) {
                    if (sDebugOptions.mShowPaths) {
                        DebugRenderer::drawWireQuadThreadSafe(truePos, f32v2(1.0f), COLOR_GREEN, DEBUG_DURATION);
                    }
                    break;
                }
                else {
                    // If this one is out of range, just continue, dont add neighbors
                    if (sDebugOptions.mShowPaths) {
                        DebugRenderer::drawWireQuadThreadSafe(truePos, f32v2(1.0f), COLOR_RED, DEBUG_DURATION);
                    }
                    continue;
                }
            }

            if (sDebugOptions.mShowPaths) {
                DebugRenderer::drawWireQuadThreadSafe(currentPos, f32v2(1.0f), COLOR_YELLOW, DEBUG_DURATION);
            }
            for (Cartesian c : cartesians) {
                const f32v3 newPos = currentPos + f32v3(CARTESIAN_NORMALS_2D[e_cast(c)].x, CARTESIAN_NORMALS_2D[e_cast(c)].y, 0.0f);
                if (closedList.find(newPos) == closedList.end()) {
                    if (glm::length2(newPos - goal) < targetRadiusSQ) {
                        openList[openListBack++] = newPos;
                        closedList.insert(newPos);
                    }
                }
            }
        }
    }

    if (startNavNodeIndex == INVALID_NAV_NODE_INDEX) {
        // Reversed on purpose
        LOG_WARN("Failed to find coarse path due to invalid goal navnode");
        path.finishedGenerating.store(true);
        return false;
    }

    PreciseTimer timer;
    
    mOpenList.clear();
    mOpenList.reserve(MAXIMUM_COARSE_NODES);
    
    const IHeightmapGrid& heightGrid = mNavWorld.getWorld().getHeightmapGrid();
    const CoarseNavGraph& startNavGraph = startNavData->coarseNavGraph;
    const CoarseNavGraph& endNavGraph = goalNavData->coarseNavGraph;
    const CoarseNavNode* startNode = &startNavGraph.getNode(startNavNodeIndex);
    const CoarseNavNode* endNode = &endNavGraph.getNode(goalNavNodeIndex);

    // Case where we are in the same node, just return the goal
    if (startNode == endNode) {
        path.allocatePath(1);
        path.points[0] = f32v3(mNavWorld.getNavDataForContainer(startHandle.containerId).getTileWorldPos(startHandle.index)) + f32v3(0.5f,0.5f,0.0f);
        path.finishedGenerating.store(true);
        return true;
    }

    const f32v3 startWorldPos = startNavData->getTileWorldPos(startHandle.index);
    const f32v3 goalWorldPos = goalNavData->getTileWorldPos(goalHandle.index);

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
    coarseAstarEdgePropagate(*startNavData, startNode, startHandle, startNavGraph, goalWorldPos, INVALID_COARSE_NODE_PARENT, 0.0f);
    // Do the A*
    while (mOpenList.size() && mTotalAstarNodes < MAXIMUM_COARSE_NODES - 256) {
        const auto& topNode = mOpenList.top();
        id = topNode.second;
        CoarseAStarNode& astarNode = sCoarseAstarNodes[id];
        
        mOpenList.pop();

        const LiteTileHandle& handle = astarNode.tileHandle;
        const ContainerNavData& navData = mNavWorld.getNavDataForContainer(handle.containerId);
        const CoarseNavGraph& navGraph = navData.coarseNavGraph;
        const ui16 navNodeIndex = navGraph.tileCoarseNavIndices[handle.index];
        if (navNodeIndex == INVALID_NAV_NODE_INDEX) {
            continue;
        }
        const CoarseNavNode* navNode = &navGraph.getNode(navNodeIndex);
        if (navNode == endNode) {
            foundGoal = true;
            break;
        }

        coarseAstarEdgePropagate(navData, navNode, handle, navGraph, goalWorldPos, id, astarNode.g);
    }

    clearCoarseClosedList();

    if (!foundGoal) {
        LOG_WARN("Failed to find coarse path in {} ms with {} total nodes checked", timer.stop(), mTotalAstarNodes);
        path.finishedGenerating.store(true);
        return false;
    }

    finishCoarsePath(startHandle, goalHandle, id, path, false/*reverse*/);
    path.targetPosition = goal;

    LOG_TRACE("Coarse path found in {} ms with {} total nodes checked", timer.stop(), mTotalAstarNodes);

    return true;
}

//
//LiteTileHandle PathFinder::tryGenerateCoarsePathToClosestFreeHarvestableSynchronous(const f32v3& start, TileHarvestable harvestable, f32 maxDistance, OUT NavPath& path) {
//    PROFILE_FUNCTION();
//    ASSERT_NAV_THREAD();
//    assert(path.numPoints == 0); // Should be uninitialized
//
//    // We pathfind forwards
//    const ContainerNavData* startNavData = nullptr;
//    LiteTileHandle startHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(glm::floor(start), &startNavData);
//
//    if (!startNavData) {
//        LOG_WARN("Failed to find coarse harvestable path due to invalid start");
//        path.finishedGenerating.store(true);
//        return LiteTileHandle();
//    }
//
//    const CoarseNavNodeIndex startNavNodeIndex = startNavData->coarseNavGraph.tileCoarseNavIndices[startHandle.index];
//
//    if (startNavNodeIndex == INVALID_NAV_NODE_INDEX) {
//        LOG_WARN("Failed to find coarse harvestable path due to invalid start navnode");
//        path.finishedGenerating.store(true);
//        return LiteTileHandle();
//    }
//
//    PreciseTimer timer;
//
//    mOpenList.clear();
//    mOpenList.reserve(MAXIMUM_COARSE_NODES);
//
//    const IHeightmapGrid& heightGrid = mNavWorld.getWorld().getHeightmapGrid();
//    const CoarseNavGraph& startNavGraph = startNavData->coarseNavGraph;
//    const CoarseNavNode* startNode = &startNavGraph.getNode(startNavNodeIndex);
//
//    const f32v3 startWorldPos = startNavData->getTileWorldPos(startHandle.index);
//
//    // A* pathfind through the coarse graph
//    mCoarseClosedList.clear();
//    mCoarseClosedList.reserve(MAXIMUM_COARSE_NODES);
//
//    mTotalAstarNodes = 0;
//    CoarseAstarNodeID id;
//
//    LiteTileHandle foundHarvestableHandle;
//
//    startNode->isClosed = true;
//    mCoarseClosedList.push_back(startNode);
//    // TODO: Optimize via a specific BFS method instead of using a bad heuristic
//    coarseAstarEdgePropagate(*startNavData, startNode, startHandle, startNavGraph, startWorldPos /*BAD HEURISTIC*/, INVALID_COARSE_NODE_PARENT, 0.0f);
//    // Do the A*
//    while (mOpenList.size() && mTotalAstarNodes < MAXIMUM_COARSE_NODES - 256) {
//        const auto& topNode = mOpenList.top();
//        id = topNode.second;
//        CoarseAStarNode& astarNode = sCoarseAstarNodes[id];
//
//        mOpenList.pop();
//        if (astarNode.g > maxDistance) {
//            continue;
//        }
//
//        const LiteTileHandle& handle = astarNode.tileHandle;
//        const ContainerNavData& navData = mNavWorld.getNavDataForContainer(handle.containerId);
//        const CoarseNavGraph& navGraph = navData.coarseNavGraph;
//        const ui16 navNodeIndex = navGraph.tileCoarseNavIndices[handle.index];
//        if (navNodeIndex == INVALID_NAV_NODE_INDEX) {
//            continue;
//        }
//        const std::set<TileIndex>* harvestablesPtr = navGraph.harvestablesLookup.tryGetHarvestables(navNodeIndex, harvestable);
//        if (harvestablesPtr && harvestablesPtr->size()) {
//            const LiteTileHandle targetHarvestable(handle.containerId, *harvestablesPtr->begin());
//            if (mNavWorld.navThreadTryReserveHarvestable(handle)) {
//                foundHarvestableHandle = targetHarvestable;
//                if (sDebugOptions.mShowPaths) {
//                    DebugRenderer::drawWireQuadThreadSafe(f32v3(navData.getTileWorldPos(foundHarvestableHandle.index)), f32v2(1.0f), COLOR_CYAN, DEBUG_DURATION * 2);
//                }
//                break;
//            }
//        }
//
//        const CoarseNavNode* navNode = &navGraph.getNode(navNodeIndex);
//        coarseAstarEdgePropagate(navData, navNode, handle, navGraph, startWorldPos /*BAD HEURISTIC*/, id, astarNode.g);
//    }
//
//    clearCoarseClosedList();
//
//    if (!foundHarvestableHandle.isValid()) {
//        LOG_TRACE("Coarse path failed in {} ms with {} total nodes checked", timer.stop(), mTotalAstarNodes);
//        path.finishedGenerating.store(true);
//        return LiteTileHandle();
//    }
//
//    finishCoarsePath(startHandle, foundHarvestableHandle, id, path, true/*reverse*/);
//
//    LOG_TRACE("Coarse path found in {} ms with {} total nodes checked", timer.stop(), mTotalAstarNodes);
//
//    return foundHarvestableHandle;
//}

class EdgeNodeHash {
public:
    size_t operator()(const std::pair<TileContainerID, ui32>& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.first);
        boost::hash_combine(seed, v.second);
        return seed;
    }
};

void PathFinder::coarseAstarEdgePropagate(const ContainerNavData& navData, const CoarseNavNode* navNode, const LiteTileHandle& tileHandle, const CoarseNavGraph& navGraph, const f32v3& goalPos, CoarseAstarNodeID parentId, f32 prevG) {
    const i32v3& containerDims = navData.containerDims;
    const TileIndex internalIndexOffsetsCartesian[4] = {
        -containerDims.x, // South
        -1,               // West
         1,               // East
        containerDims.x   // North
    };

    const i32v3 tilePos = navData.getTileWorldPos(tileHandle.index);
    // Iterate all edges
    for (ui16 i = 0; i < navNode->edgeCount; ++i) {
        CoarseNavNodeEdge& edge = navGraph.edges[navNode->edgesStart + i];
        if (edge.isExternalEdge()) {
            // With external edges we have to look up the adjacent nav nodes
            typedef std::pair<TileContainerID, ui32 /*navNode*/> EdgeKey;
            std::unordered_map<EdgeKey, TileIndex, EdgeNodeHash> edgeNodes;
            edgeNodes.reserve(4); // Usually quite small, can we use a better data structure?
            i32v3 edgeStartPosWorld = navData.worldPos + navData.getTileXYZOffsetWithZScale(edge.startPos);
            // Collect all valid external nodes along this edge
            for (int i = 0; i < (int)edge.edgeLength; ++i) {
                const i32v3& edgeDir = CARTESIAN_TANGENTS_ABS_3D[e_cast(edge.dir)];
                const i32v3 edgePosWorld = edgeStartPosWorld + edgeDir * i;
                const TileIndex nextIndex = edge.startPos + (edgeDir.x + edgeDir.y * containerDims.y) * i;
                i32v3 worldPosOuter = edgePosWorld + CARTESIAN_NORMALS_3D[e_cast(edge.dir)];
                worldPosOuter.z = (i32)glm::round((f32)worldPosOuter.z + navData.fineNavGraph[nextIndex].zPositionOffsetFromFloor);
                // Get the outer tile along this coarse edge point
                const ContainerNavData* outerNavData = nullptr;
                LiteTileHandle outerTileHandle = mNavWorld.getTileHandleAndNavDataAtWorldPos(worldPosOuter, &outerNavData);
                if (!outerTileHandle.isValid()) {
                    continue;
                }
                assert(outerNavData);
                // This can happen if we have a ledge, failure case
                // TODO: Evaluate if we can mark these ledges as internal
                if (outerTileHandle.containerId == navData.containerId) {
                    /* LOG_CRITICAL("outerTileHandle.containerId == navData.containerId in PathFinder::coarseAStarEdgePropagate.Adding red debug draw line to world at edge");
                     DebugRenderer::drawLineBetweenPointsThreadSafe(edgePosWorld, worldPosOuter, color4(1.0f, 0.0f, 0.0f, 0.6f), 200000);
                     DebugRenderer::drawWireQuadThreadSafe(worldPosOuter, f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.6f), 200000);
                     DebugRenderer::drawLineBetweenPointsThreadSafe(edgePosWorld, edgePosWorld + CARTESIAN_NORMALS_3D[e_cast(edge.dir)], color4(0.0f, 0.0f, 1.0f, 0.6f), 200000);
                     __debugbreak();*/
                    continue;
                }
                // Add this as a new valid node if needed
                // This always picks first position for node due to overwrites, but thats OK we will let fine nav / steering do
                // string pulling to improve the path
                CoarseNavNodeIndex navIndex = outerNavData->coarseNavGraph.tileCoarseNavIndices[outerTileHandle.index];
                if (navIndex == INVALID_NAV_NODE_INDEX) {
                    continue;
                }
                assert(navIndex < outerNavData->coarseNavGraph.numNodes);
                EdgeKey edgeKey = std::make_pair(outerTileHandle.containerId, navIndex);
                UINT16_MAX;
                auto&& it = edgeNodes.find(edgeKey);
                if (it == edgeNodes.end()) {
                    edgeNodes.insert(std::make_pair(edgeKey, outerTileHandle.index));
                }
            }
            for (auto&& it : edgeNodes) {
                //TileContainer* adjContainer = TileContainerRepository::getTileContainer(it.first);
                const TileContainerID nextContainerId = it.first.first;
                // TODO: It might be cheaper to cache this, since we are doing it already in the above pass ^
                const ContainerNavData& nextNavData = mNavWorld.getNavDataForContainer(nextContainerId);
                const CoarseNavGraph& adjNavGraph = nextNavData.coarseNavGraph;
                const CoarseNavNode& nextNode = adjNavGraph.getNode(it.first.second);
                if (nextNode.isClosed) {
                    continue;
                }

                // Add position and node to the lists as below
                nextNode.isClosed = true;
                mCoarseClosedList.push_back(&nextNode);

                const CoarseAstarNodeID newId = mTotalAstarNodes++;
                CoarseAStarNode& newAstarNode = sCoarseAstarNodes[newId];
                const i32v3 nextPos = nextNavData.getTileWorldPos(it.second);
                newAstarNode.tileHandle = LiteTileHandle(nextContainerId, it.second);
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
                nextTileIndex -= containerDims.x * containerDims.y;
            }
            else if (edge.edgeType == TileCoarseNavEdgeType::UP) {
                nextTileIndex += containerDims.x * containerDims.y;
            }
            const CoarseAstarNodeID newId = mTotalAstarNodes++;
            CoarseAStarNode& newAstarNode = sCoarseAstarNodes[newId];
            newAstarNode.tileHandle = LiteTileHandle(tileHandle.containerId, nextTileIndex);
            const i32v3 nextPos = navData.getTileWorldPos(newAstarNode.tileHandle.index);
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

void PathFinder::clearCoarseClosedList() {
    for (auto&& node : mCoarseClosedList) {
        node->isClosed = false;
    }
}


void PathFinder::finishCoarsePath(LiteTileHandle startHandle, LiteTileHandle goalHandle, CoarseAstarNodeID lastId, NavPath& path, bool reverse) {
    ui32 pathSize = 0;
    {
        sPathPointBuffer[pathSize++] = goalHandle;
        CoarseAstarNodeID parentId = lastId;
        // Find out the path size and cache the points
        while (parentId != INVALID_COARSE_NODE_PARENT) {
            CoarseAStarNode& astarNode = sCoarseAstarNodes[parentId];
            sPathPointBuffer[pathSize++] = astarNode.tileHandle;
            parentId = astarNode.parentIndex;
        }
    }

    // Copy the path
    if (reverse) {
        path.allocatePath(pathSize);
        for (int i = 0; i < (int)pathSize; ++i) {
            LiteTileHandle& handle = sPathPointBuffer[pathSize - i - 1];
            path.points[i] = f32v3(mNavWorld.getNavDataForContainer(handle.containerId).getTileWorldPos(handle.index)) + f32v3(0.5f, 0.5f, 0.0f);
        }
    } else {
        // Append goal if it isn't at the endpoint already (Reversed)
        // TODO: ehhhhhh why not just always path forwards
        LiteTileHandle goalLiteHandle = startHandle;
        if (sPathPointBuffer[pathSize - 1] != goalLiteHandle) {
            sPathPointBuffer[pathSize++] = goalLiteHandle;
        }
        path.allocatePath(pathSize);

        for (int i = 0; i < (int)pathSize; ++i) {
            LiteTileHandle& handle = sPathPointBuffer[i];
            path.points[i] = f32v3(mNavWorld.getNavDataForContainer(handle.containerId).getTileWorldPos(handle.index)) + f32v3(0.5f, 0.5f, 0.0f);
        }
    }

    if (sDebugOptions.mShowPaths) {
        for (ui32 i = 1; i < path.numPoints; ++i) {
            const f32v3& pa = path.points[i - 1];
            const f32v3& pb = path.points[i];
            DebugRenderer::drawLineBetweenPointsThreadSafe(pa, pb, color4(1.0f, 1.0f, 0.0f, 0.6f), DEBUG_DURATION);
        }
    }
    path.finishedGenerating.store(true);
}