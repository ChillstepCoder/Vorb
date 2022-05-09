#include "stdafx.h"
#include "PathFinder.h"

#include "World.h"
#include "resources/TileRepository.h"

#include "pathfinding/NavGraph.h"

#include "options/DebugOptions.h"

#include "DebugRenderer.h"

constexpr ui32 MAX_PATH_LENGTH = 128; //255;
constexpr ui8 INVALID_PARENT = 0;

constexpr ui32 MAX_OPEN_LIST_SIZE = MAX_PATH_LENGTH * 8;

constexpr ui32 HALF_LOOKUP_LIST_WIDTH = MAX_PATH_LENGTH;
constexpr ui32 LOOKUP_LIST_WIDTH = HALF_LOOKUP_LIST_WIDTH * 2;
constexpr ui32 LOOKUP_LIST_SIZE = SQ(LOOKUP_LIST_WIDTH);

constexpr ui32 DEBUG_DURATION = 200;
inline f32v3 helperGet3DPoint(const WorldGrid& worldGrid, const f32v2& pos2d) {
    return f32v3(pos2d.x, pos2d.y, worldGrid.tryComputeHeightAtPoint(pos2d));
}
// Position and index can be inferred
struct AStarNode {
    f32 h; // Heuristic distance to target
    ui16 g; // Movement cost to the node
    ui8 parentDir; // Cartesian (+1) direction to parent
    struct {
        bool isInClosedList : 1;
        bool isInOpenList : 1;
        ui8 padding : 6;
    };

    f32 getScore() const { return (f32)g / 10.0f + h; }
};
static_assert(sizeof(AStarNode) == 8);

struct CoarseAStarNode {
    f32 h; // Heuristic distance to target
    f32 g; // Movement cost to the node
    ui16 parentIndex;
    PathPoint position;

    f32 getScore() const { return g + h; }
};

constexpr CoarseAstarNodeID MAXIMUM_COARSE_NODES = 8196;
thread_local CoarseAStarNode sCoarseAstarNodes[MAXIMUM_COARSE_NODES];

// No allocations baby
constexpr ui32 MAX_PATH_BUFFER_SIZE = MAX_PATH_LENGTH * 16;
thread_local AStarNode sNodes[LOOKUP_LIST_SIZE] = {};
thread_local PathPoint sPathPointBuffer[MAX_PATH_LENGTH * 16];

static_assert(LOOKUP_LIST_SIZE <= UINT16_MAX + 1);
typedef ui16 AStarNodeID;

struct NodeListElement {
    AStarNodeID nodeIndex;
    ui16 score;
};

class NodeList {
public:
    // TODO: Insertion sort on add
    void add(AStarNodeID index, ui16 score) {
        assert(mSize < MAX_OPEN_LIST_SIZE);
        mNodes[mEnd++] = { index, score };
        if (mEnd >= MAX_OPEN_LIST_SIZE) mEnd = 0;
        ++mSize;
    }

    // TODO: I think we could have a sorted array that is easy to sort by storing the counts of each node weight
    // and having an array of data with entries for each integer node score, then we can build sort offsets for each
    // group, and increment a secondary offset array (is this faster than linear search?) (heap sort?)
    NodeListElement popLowestScoreNode() {
        assert(mStart != mEnd);
        // Reverse iterate to favor newer nodes
        ui32 lastNode = (mEnd == 0 ? MAX_OPEN_LIST_SIZE - 1 : mEnd - 1);
        ui32 iter = lastNode;
        ui32 best = iter;
        f32 bestScore = mNodes[iter].score;

        // Find best score
        while (true) {
            NodeListElement& node = mNodes[iter];
            if (node.score < bestScore) {
                bestScore = node.score;
                best = iter;
            }
            if (iter == mStart) {
                break;
            }
            iter = (iter == 0 ? MAX_OPEN_LIST_SIZE - 1 : iter - 1);
        }
        // Store best
        NodeListElement rv = mNodes[best];
        // Pop node
        mNodes[best] = mNodes[lastNode];
        mEnd = lastNode;
        // Shrink ring buffer
        --mSize;
        return rv;
    }

    void replaceScore(ui32 nodeIndex, f32 score) {
        ui32 iter = mStart + 1;
        // TODO: with an unsigned byte this could be automatic via overflow
        if (iter >= MAX_OPEN_LIST_SIZE) iter = 0;

        // Find best score
        while (iter != mEnd) {
            NodeListElement& node = mNodes[iter];
            if (node.nodeIndex == nodeIndex) {
                node.score = score;
                return;
            }
            if (++iter >= MAX_OPEN_LIST_SIZE) iter = 0;
        }
    }

    ui32 size() { return mSize; }
private:
    NodeListElement mNodes[MAX_OPEN_LIST_SIZE] = {};
    ui32 mSize = 0;
    ui32 mStart = 0;
    ui32 mEnd = 0;
};


// http://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html
f32 getDiagonalHeuristicAtPosition(const PathPoint& node, const PathPoint& goal) {

    //constexpr f32 D = 1;
    constexpr f32 D2 = 1.4f;

    // When D = 1 and D2 = 1, this is called the Chebyshev distance.
    // When D = 1 and D2 = sqrt(2) (1.4), this is called the octile distance.
    // TODO: Truncation error for large integers?? (i dont remember this)
    f32 dx = abs((f32)node.x - (f32)goal.x);
    f32 dy = abs((f32)node.y - (f32)goal.y);
    return /*D * */(dx + dy) + (D2 - 2.0f/* * D*/) * vmath::min(dx, dy);
}

f32 getEuclideanHeuristicAtPosition(const PathPoint& node, const PathPoint& goal) {
    return glm::length(f32v2(node.xy) - f32v2(goal.xy));
}

AStarNodeID getNodeIndex(const PathPoint node, const PathPoint bottomLeft) {
    assert(node.x >= bottomLeft.x && node.y >= bottomLeft.y);
    const PathPoint offset = node.xy - bottomLeft.xy;
    return (AStarNodeID)(offset.y * LOOKUP_LIST_WIDTH + offset.x);
}

PathPoint nodeIndexToWorldPos(ui32 nodeIndex, const PathPoint& bottomLeft) {
    return PathPoint(bottomLeft.x + nodeIndex % LOOKUP_LIST_WIDTH, bottomLeft.y + nodeIndex / LOOKUP_LIST_WIDTH);
}

AStarNode& getNodeLookup(const ui32 nodeIndex) {
    return sNodes[nodeIndex];
}

AStarNode& getNodeLookup(const PathPoint& node, const PathPoint& bottomLeft) {
    return sNodes[getNodeIndex(node, bottomLeft)];
}

enum NodeParentDir {
    NODE_DIR_NONE       = 0,
    NODE_DIR_DOWN_LEFT  = 1,
    NODE_DIR_DOWN       = 2,
    NODE_DIR_DOWN_RIGHT = 3,
    NODE_DIR_LEFT       = 4,
    NODE_DIR_RIGHT      = 5,
    NODE_DIR_UP_LEFT    = 6,
    NODE_DIR_UP         = 7,
    NODE_DIR_UP_RIGHT   = 8,
};
constexpr NodeParentDir OPPOSITE_NODE_DIRS[9] = {
   NODE_DIR_NONE,       // NODE_DIR_NONE
   NODE_DIR_UP_RIGHT,   // NODE_DIR_DOWN_LEFT
   NODE_DIR_UP,         // NODE_DIR_DOWN
   NODE_DIR_UP_LEFT,    // NODE_DIR_DOWN_RIGHT
   NODE_DIR_RIGHT,      // NODE_DIR_LEFT
   NODE_DIR_LEFT,       // NODE_DIR_RIGHT
   NODE_DIR_DOWN_RIGHT, // NODE_DIR_UP_LEFT
   NODE_DIR_DOWN,       // NODE_DIR_UP
   NODE_DIR_DOWN_LEFT,  // NODE_DIR_UP_RIGHT
};

constexpr ui32 MOVE_COST_STRAIGHT = 10;
constexpr ui32 MOVE_COST_DIAGONAL = 14;
const ui32 MOVEMENT_COSTS[9] = {
   0.0f,               // NODE_DIR_NONE
   MOVE_COST_DIAGONAL, // NODE_DIR_DOWN_LEFT
   MOVE_COST_STRAIGHT, // NODE_DIR_DOWN
   MOVE_COST_DIAGONAL, // NODE_DIR_DOWN_RIGHT
   MOVE_COST_STRAIGHT, // NODE_DIR_LEFT
   MOVE_COST_STRAIGHT, // NODE_DIR_RIGHT
   MOVE_COST_DIAGONAL, // NODE_DIR_UP_LEFT
   MOVE_COST_STRAIGHT, // NODE_DIR_UP
   MOVE_COST_DIAGONAL, // NODE_DIR_UP_RIGHT
};

const i32v2 NODE_OFFSETS[9] = {
   i32v2( 0,  0),  // NODE_DIR_NONE
   i32v2(-1, -1), // NODE_DIR_DOWN_LEFT
   i32v2( 0, -1), // NODE_DIR_DOWN
   i32v2( 1, -1), // NODE_DIR_DOWN_RIGHT
   i32v2(-1,  0), // NODE_DIR_LEFT
   i32v2( 1,  0), // NODE_DIR_RIGHT
   i32v2(-1,  1), // NODE_DIR_UP_LEFT
   i32v2( 0,  1), // NODE_DIR_UP
   i32v2( 1,  1), // NODE_DIR_UP_RIGHT
};

const i32v2 NODE_CORNER_NEIGHBORS[9] = {
   i32v2(0,  0),                         // NODE_DIR_NONE
   i32v2(NODE_DIR_DOWN, NODE_DIR_LEFT),  // NODE_DIR_DOWN_LEFT
   i32v2(0,  0),                         // NODE_DIR_DOWN
   i32v2(NODE_DIR_DOWN, NODE_DIR_RIGHT), // NODE_DIR_DOWN_RIGHT
   i32v2(0,  0),                         // NODE_DIR_LEFT
   i32v2(0,  0),                         // NODE_DIR_RIGHT
   i32v2(NODE_DIR_UP,   NODE_DIR_LEFT),  // NODE_DIR_UP_LEFT
   i32v2(0,  1),                         // NODE_DIR_UP
   i32v2(NODE_DIR_UP,   NODE_DIR_RIGHT), // NODE_DIR_UP_RIGHT
};

// TODO: https://gamedev.stackexchange.com/questions/94148/pathfinding-tile-based-navigation-mesh

// https://github.com/daancode/a-star/blob/master/source/AStar.cpp
bool PathFinder::generateFinePathSynchronous(const World& world, const PathPoint& start, const PathPoint& goal, OUT NavPath& path) {
    assert(path.numPoints == 0); // Should be uninitialized
    // Only runs on nav thread
    assert(IS_NAV_THREAD());
    // TODO: Profiling
    PreciseTimer timer;
    const WorldGrid& worldGrid = world.getWorldGrid();

    ui32 debugCount = 0;

    // Make sure we can fit in nodes array
    // We pathfind backwards
    ui32 dx = (ui32)abs((i32)goal.x - (i32)start.x);
    ui32 dy = (ui32)abs((i32)goal.y - (i32)start.y);
    if (dx >= HALF_LOOKUP_LIST_WIDTH || dy >= HALF_LOOKUP_LIST_WIDTH) {
        path.finishedGenerating.store(true);
        return false;
    }
    // Clear out the nodes
    // TODO: Is this faster or slower than using a closed list?
    memset(sNodes, 0, sizeof(AStarNode) * LOOKUP_LIST_SIZE);

    // This must remain sorted
    NodeList openList;

    const PathPoint bottomLeftPoint = goal - PathPoint(HALF_LOOKUP_LIST_WIDTH, HALF_LOOKUP_LIST_WIDTH);

    // Add start node to the open list
    const AStarNodeID startNodeIndex = getNodeIndex(goal, bottomLeftPoint);
    AStarNode& startNode = getNodeLookup(startNodeIndex);
    startNode.isInOpenList = true;
    startNode.g = 0;
    startNode.h = getDiagonalHeuristicAtPosition(goal, start);
    openList.add(startNodeIndex, startNode.getScore());

    int TOTAL = 0;

    const ui16 goalNodeIndex = getNodeIndex(start, bottomLeftPoint);
    bool foundGoal = false;
    while (openList.size() && openList.size() < MAX_OPEN_LIST_SIZE) {
        ++TOTAL;
        // Pull best node off of the open list (Linear search)
        const ui16 nodeIndex = openList.popLowestScoreNode().nodeIndex;
        if (nodeIndex == goalNodeIndex) {
            foundGoal = true;
            break;
        }

        AStarNode& node = getNodeLookup(nodeIndex);
        // Add current node to implicit closed list
        node.isInOpenList = false;
        node.isInClosedList = true;

        const PathPoint nodePoint = nodeIndexToWorldPos(nodeIndex, bottomLeftPoint);
        const Tile* startTile = world.tryGetTileAtWorldPos(nodePoint.xy);
        assert(startTile);
        const f32 startBaseZPosition = startTile->getBaseZPositionUncompressedThreadSafe();
        
        /*if (sDebugOptions.mShowPaths) {
            if (debugCount > 255) debugCount = 0;
            f32v3 point3d = helperGet3DPoint(worldGrid, f32v2(nodePoint));
            DebugRenderer::drawFilledQuad(point3d, f32v2(1.0f), color4(debugCount++ / 255.0f, node.h / 128.0f, 0.0f, 0.2f), DEBUG_DURATION, 0);
        }*/

        // Precompute collision weights and points for neighbors
        PathPoint nextPoints[8];
        f32 pathWeights[8];
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            const i32v2& offset = NODE_OFFSETS[dir];
            const int i = dir - 1;
            // TODO: This will result in a math error when casting the ui32 to i32 truncates large integers
            const PathPoint& nextPoint = nextPoints[i] = PathPoint((i32)nodePoint.x + offset.x, (i32)nodePoint.y + offset.y);

            // Bounds check
            if (nextPoint.x < bottomLeftPoint.x ||
                nextPoint.y < bottomLeftPoint.y ||
                nextPoint.x > bottomLeftPoint.x + LOOKUP_LIST_WIDTH ||
                nextPoint.y > bottomLeftPoint.y + LOOKUP_LIST_WIDTH) {
                pathWeights[i] = 0.0f;
                continue;
            }
            const Tile* tile = world.tryGetTileAtWorldPos(nextPoint.xy);
            assert(tile);
            f32 weight = (tile->getPathWeightNavThread() / 255.0f);
            const f32 baseZPosition = tile->getBaseZPositionUncompressedThreadSafe();
            if (baseZPosition >= startBaseZPosition + 2) {
                // Too tall!
                pathWeights[i] = 0.0f;
            } else if (baseZPosition >= startBaseZPosition + 1) {
                // Upward
                pathWeights[i] = weight * 0.5f;
            }
            else {
                // Standard or downward
                pathWeights[i] = weight;
            }

            // ROADS ARE WORTH MORE
           /* if (tile->hasFlagThreadSafe(TileFlags::TILE_FLAG_ROAD)) {
                pathWeights[i] *= 2.0f;
            }*/
        }

        // Check neighbors
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            const int i = dir - 1;
            const PathPoint& nextPoint = nextPoints[i];
            // Check if we can move here
            if (pathWeights[i] > 0.0001f) {

                // For diagonal nodes, make sure one both neighbors is clear
                if (MOVEMENT_COSTS[dir] == MOVE_COST_DIAGONAL) {
                    const i32v2& neighbors = NODE_CORNER_NEIGHBORS[dir];
                    if (pathWeights[neighbors.x] <= 0.0001f ||
                        pathWeights[neighbors.y] <= 0.0001f) {
                        continue;
                    }
                }
                const ui32 nextNodeIndex = getNodeIndex(nextPoint, bottomLeftPoint);
                AStarNode& nextNode = getNodeLookup(nextNodeIndex);

                const ui32 cost = node.g + MOVEMENT_COSTS[dir] / pathWeights[i];
                if (nextNode.isInOpenList) {
                    if (cost < nextNode.g) {
                        // Reparent the node
                        nextNode.g = cost;
                        nextNode.parentDir = OPPOSITE_NODE_DIRS[dir];
                        openList.replaceScore(nextNodeIndex, nextNode.getScore());
                    }
                }
                else if (nextNode.isInClosedList) {
                    if (cost < nextNode.g) {
                        nextNode.g = cost;
                        nextNode.h = getDiagonalHeuristicAtPosition(nextPoint, start);
                        // This could be a single assignment since bitfield
                        nextNode.isInClosedList = false;
                        nextNode.isInOpenList = true;
                        openList.add(nextNodeIndex, nextNode.getScore());
                    }
                }
                else {
                    nextNode.g = cost;
                    nextNode.h = getDiagonalHeuristicAtPosition(nextPoint, start);
                    nextNode.parentDir = OPPOSITE_NODE_DIRS[dir];
                    nextNode.isInOpenList = true;
                    openList.add(nextNodeIndex, nextNode.getScore());
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
    AStarNode& goalNode = getNodeLookup(goalNodeIndex);

    ui32 pathSize = 0;
    {
        AStarNode* node = &goalNode;
        PathPoint worldPoint = nodeIndexToWorldPos(goalNodeIndex, bottomLeftPoint);

        // Find out the path size and cache the points
        while (node != &startNode) {
            sPathPointBuffer[pathSize++] = worldPoint;
            const i32v2& offsetToParent = NODE_OFFSETS[node->parentDir];
            worldPoint = PathPoint((i32)worldPoint.x + offsetToParent.x, (i32)worldPoint.y + offsetToParent.y);
            node = &getNodeLookup(worldPoint, bottomLeftPoint);
        }
    }

    // TODO: Path memory pool);
    assert(pathSize < MAX_PATH_BUFFER_SIZE);
    path.allocatePath(pathSize);
    // Copy the path
    memcpy(path.points, sPathPointBuffer, pathSize * sizeof(PathPoint));

    std::cout << "Generated path in " << timer.stop() << " ms with " << TOTAL << " nodes checked\n";
    path.finishedGenerating.store(true);
    return true;
}


void coarseAstarEdgePropagate(const World& world, const NavNode* navNode, CoarseClosedList& closedList, CoarseAstarNodeID& totalAstarNodes, CoarseAStarNode* astarNodes, const PathPoint& goal, CoarseOpenList& openList, CoarseAstarNodeID parentId, f32 prevG, const PathPoint& parentPos) {
    const WorldGrid& worldGrid = world.getWorldGrid();
    const Chunk& chunk = worldGrid.getChunk(navNode->chunkId);
    PathPoint chunkWorldPos = PathPoint(chunk.getWorldPos());
    // Iterate all edges
    const PathPoint cornerWorldPos = chunkWorldPos + PathPoint(navNode->cornerPos.getX(), navNode->cornerPos.getY());
    for (ui32 cartesian = 0; cartesian < 4; ++cartesian) {
        ui32 count = navNode->counts[cartesian];
        for (ui32 edgeIndex = 0; edgeIndex < count; ++edgeIndex) {
            const LiteNavNodeEdge& edge = navNode->edges[cartesian][edgeIndex];
            const PathPoint edgeOffset = PathPoint(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (ui16)edge.start;

            PathPoint position = cornerWorldPos + edgeOffset;
            // Offset into next cell
            position = PathPoint(i32v2(position.xy) + CARTESIAN_NORMALS[cartesian]);
            // Offset to center of edge
            position += PathPoint(f32v2(CARTESIAN_EDGE_DIRS_ABS[cartesian]) * (f32)(edge.lengthMinusOne + 1.0f) * 0.5f) + NAV_NODE_EDGE_OFFSETS[cartesian];
            const NavNode* nextNode = world.tryGetNavNodeAtWorldPos(position.xy);
            if (!nextNode || nextNode->isClosed) {
                // TODO: Update G if better?
                continue;
            }
            // Right now we are deliberately not allowing path to be re-updated
            nextNode->isClosed = true;
            closedList.push_back(nextNode);

            CoarseAstarNodeID newId = totalAstarNodes++;
            CoarseAStarNode& node = astarNodes[newId];
            node.position = position;
            node.g = prevG + glm::length(f32v2(node.position.xy) - f32v2(parentPos.xy));
            node.h = getEuclideanHeuristicAtPosition(node.position, goal);
            if (sDebugOptions.mShowPaths) {
                f32v3 pos1 = helperGet3DPoint(worldGrid, f32v2(node.position.xy));
                f32v3 pos2 = helperGet3DPoint(worldGrid, f32v2(parentPos.xy));
                DebugRenderer::drawLineBetweenPointsThreadSafe(pos1, pos2, color4(((int)node.g % 255) / 255.0f, ((int)node.h % 255) / 255.0f, 1.0f, 0.5f), DEBUG_DURATION);
            }
            node.parentIndex = parentId;
            openList.push(std::make_pair(node.getScore(), newId));
        }
    }
}

bool PathFinder::generateCoarsePathSynchronous(const World& world, const PathPoint& start, const PathPoint& goal, OUT NavPath& path)
{
    assert(path.numPoints == 0); // Should be uninitialized
    PreciseTimer timer;
    
    mOpenList.clear();
    mOpenList.reserve(MAXIMUM_COARSE_NODES);
    
    const WorldGrid& worldGrid = world.getWorldGrid();

    // We pathfind backwards
    const NavNode* startNode = world.tryGetNavNodeAtWorldPos(goal.xy);
    if (!startNode) {
        //pError("Error: Failed to find coarse path due to invalid start\n");
        path.finishedGenerating.store(true);
        return false;
    }
    const NavNode* endNode = world.tryGetNavNodeAtWorldPos(start.xy);
    if (!endNode) {
        //pError("Error: Failed to find coarse path due to invalid end\n");
        path.finishedGenerating.store(true);
        return false;
    }

    // Case where we are in the same node, just return the goal
    if (startNode == endNode) {
        path.allocatePath(1);
        path.points[0] = goal;
        path.finishedGenerating.store(true);
        return true;
    }

    // A* pathfind through the coarse graph
    // TODO: non arbitrary reserve
    //sCoarseClosedList.clear();
    //sCoarseClosedList.reserve(MAXIMUM_COARSE_NODES); // TODO: Move this to an init?
    //sCoarseClosedList.insert(nullptr); // So we dont need explicit null check later
    mCoarseClosedList.clear();
    mCoarseClosedList.reserve(MAXIMUM_COARSE_NODES);

    CoarseAstarNodeID totalAstarNodes = 0;
    CoarseAstarNodeID id;
    bool foundGoal = false;

    // TODO: Race conditions with the navgraph generator thread?
    // Add all first edges to the open and closed lists
    //sCoarseClosedList.insert(startNode);
    startNode->isClosed = true;
    mCoarseClosedList.push_back(startNode);
    coarseAstarEdgePropagate(world, startNode, mCoarseClosedList, totalAstarNodes, sCoarseAstarNodes, start, mOpenList, INVALID_COARSE_NODE_PARENT, 0.0f, goal);
    // Do the A*
    while (mOpenList.size() && totalAstarNodes < MAXIMUM_COARSE_NODES - 256) {
        const auto& topNode = mOpenList.top();
        id = topNode.second;
        CoarseAStarNode& astarNode = sCoarseAstarNodes[id];
        mOpenList.pop();
        const NavNode* navNode = world.tryGetNavNodeAtWorldPos(astarNode.position.xy);
        if (navNode == endNode) {
            foundGoal = true;
            break;
        }

        coarseAstarEdgePropagate(world, navNode, mCoarseClosedList, totalAstarNodes, sCoarseAstarNodes, start, mOpenList, id, astarNode.g, astarNode.position);
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
        path.finishedGenerating.store(true);
        return false;
    }

    ui32 pathSize = 0;
    {
        sPathPointBuffer[pathSize++] = start;
        CoarseAstarNodeID parentId = id;
        // Find out the path size and cache the points
        while (parentId != INVALID_COARSE_NODE_PARENT) {
            CoarseAStarNode& astarNode = sCoarseAstarNodes[parentId];
            sPathPointBuffer[pathSize++] = astarNode.position;
            parentId = astarNode.parentIndex;
        }
    }

    // Append goal if it isnt at the endpoint already
    if (sPathPointBuffer[pathSize - 1].xy != goal.xy) {
        sPathPointBuffer[pathSize++] = goal;
    }

    // TODO: Path memory pool
    path.allocatePath(pathSize);
    // Copy the path
    memcpy(path.points, sPathPointBuffer, pathSize * sizeof(PathPoint));


    if (sDebugOptions.mShowPaths) {
        for (ui32 i = 1; i < path.numPoints; ++i) {
            const PathPoint& a = path.points[i - 1];
            const PathPoint& b = path.points[i];
            f32v3 pointA3d = helperGet3DPoint(worldGrid, f32v2(a.xy));
            f32v3 pointB3d = helperGet3DPoint(worldGrid, f32v2(b.xy));
            DebugRenderer::drawLineBetweenPointsThreadSafe(pointA3d, pointB3d, color4(1.0f, 1.0f, 0.0f, 0.6f), DEBUG_DURATION);
        }
    }

        // Back propagation
    std::cout << "Coarse path found in " << timer.stop() << "ms with " << totalAstarNodes << " total nodes checked\n";
    path.finishedGenerating.store(true);
    return true;
}
