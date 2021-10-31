#include "stdafx.h"
#include "PathFinder.h"

#include "World.h"
#include "world/TileRepository.h"

#include "DebugRenderer.h"
#define PATH_DEBUG 1

constexpr ui32 MAX_PATH_LENGTH = 128; //255;
constexpr ui8 INVALID_PARENT = 0;

constexpr ui32 MAX_OPEN_LIST_SIZE = MAX_PATH_LENGTH * 8;

constexpr ui32 HALF_LOOKUP_LIST_WIDTH = MAX_PATH_LENGTH;
constexpr ui32 LOOKUP_LIST_WIDTH = HALF_LOOKUP_LIST_WIDTH * 2;
constexpr ui32 LOOKUP_LIST_SIZE = SQ(LOOKUP_LIST_WIDTH);

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

typedef ui16 CoarseAstarNodeID;
constexpr ui16 INVALID_COARSE_NODE_PARENT = UINT16_MAX;
static_assert(sizeof(CoarseAstarNodeID) == sizeof(ui16), "Update invalid parent");

struct CoarseAStarNode {
    f32 h; // Heuristic distance to target
    f32 g; // Movement cost to the node
    ui16 parentIndex;
    ui32v2 position;

    f32 getScore() const { return g + h; }
};

constexpr CoarseAstarNodeID MAXIMUM_COARSE_NODES = 8196;
thread_local CoarseAStarNode sCoarseAstarNodes[MAXIMUM_COARSE_NODES];
thread_local std::unordered_set<const NavNode*> sCoarseClosedList;

// No allocations baby
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
f32 getDiagonalHeuristicAtPosition(const ui32v2& node, const ui32v2& goal) {

    //constexpr f32 D = 1;
    constexpr f32 D2 = 1.4f;

    // When D = 1 and D2 = 1, this is called the Chebyshev distance.
    // When D = 1 and D2 = sqrt(2) (1.4), this is called the octile distance.
    // TODO: Truncation error for large integers?? (i dont remember this)
    f32 dx = abs((f32)node.x - (f32)goal.x);
    f32 dy = abs((f32)node.y - (f32)goal.y);
    return /*D * */(dx + dy) + (D2 - 2.0f/* * D*/) * vmath::min(dx, dy);
}

f32 getEuclideanHeuristicAtPosition(const ui32v2& node, const ui32v2& goal) {
    return glm::length(f32v2(node) - f32v2(goal));
}

AStarNodeID getNodeIndex(const i32v2 node, const i32v2 bottomLeft) {
    assert(node.x >= bottomLeft.x && node.y >= bottomLeft.y);
    const i32v2 offset = node - bottomLeft;
    return (AStarNodeID)(offset.y * LOOKUP_LIST_WIDTH + offset.x);
}

ui32v2 nodeIndexToWorldPos(ui32 nodeIndex, const ui32v2& bottomLeft) {
    return ui32v2(bottomLeft.x + nodeIndex % LOOKUP_LIST_WIDTH, bottomLeft.y + nodeIndex / LOOKUP_LIST_WIDTH);
}

AStarNode& getNodeLookup(const ui32 nodeIndex) {
    return sNodes[nodeIndex];
}

AStarNode& getNodeLookup(const ui32v2& node, const ui32v2& bottomLeft) {
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
std::unique_ptr<Path> PathFinder::generatePathSynchronous(const World& world, const ui32v2& start, const ui32v2& goal) {
    // TODO: Profiling
    PreciseTimer timer;

    ui32 debugCount = 0;

    // Make sure we can fit in nodes array
    // We pathfind backwards
    ui32 dx = (ui32)abs((i32)goal.x - (i32)start.x);
    ui32 dy = (ui32)abs((i32)goal.y - (i32)start.y);
    if (dx >= HALF_LOOKUP_LIST_WIDTH || dy >= HALF_LOOKUP_LIST_WIDTH) {
        return nullptr;
    }
    // Clear out the nodes
    // TODO: Is this faster or slower than using a closed list?
    memset(sNodes, 0, sizeof(AStarNode) * LOOKUP_LIST_SIZE);

    // This must remain sorted
    NodeList openList;

    const ui32v2 bottomLeftPoint = goal - ui32v2(HALF_LOOKUP_LIST_WIDTH, HALF_LOOKUP_LIST_WIDTH);

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

        const ui32v2 nodePoint = nodeIndexToWorldPos(nodeIndex, bottomLeftPoint);
        const TileCollision startCollision = world.getTileCollisionAtWorldPos(nodePoint);
        
#if PATH_DEBUG == 1
        if (debugCount > 255) debugCount = 0;
        DebugRenderer::drawFilledQuad(nodePoint, f32v2(1.0f), color4(debugCount++ / 255.0f, node.h / 128.0f, 0.0f, 0.2f), 200, 0);
#endif

        // Precompute collision weights and points for neighbors
        ui32v2 nextPoints[8];
        f32 pathWeights[8];
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            const i32v2& offset = NODE_OFFSETS[dir];
            const int i = dir - 1;
            // TODO: This will result in a math error when casting the ui32 to i32 truncates large integers
            const ui32v2& nextPoint = nextPoints[i] = ui32v2((i32)nodePoint.x + offset.x, (i32)nodePoint.y + offset.y);

            // Bounds check
            if (nextPoint.x < bottomLeftPoint.x ||
                nextPoint.y < bottomLeftPoint.y ||
                nextPoint.x > bottomLeftPoint.x + LOOKUP_LIST_WIDTH ||
                nextPoint.y > bottomLeftPoint.y + LOOKUP_LIST_WIDTH) {
                pathWeights[i] = 0.0f;
                continue;
            }
            TileCollision collision = world.getTileCollisionAtWorldPos(nextPoint);
            f32 weight = (collision.pathWeight / 255.0f);
            if (collision.baseZPosition == startCollision.baseZPosition + 1) {
                // Upward
                pathWeights[i] = (collision.pathWeight / 255.0f) * 0.5f;
            }
            else if (collision.baseZPosition >= startCollision.baseZPosition + 2) {
                // Too tall!
                pathWeights[i] = 0.0f;
            }
            else {
                // Standard or downward
                pathWeights[i] = (collision.pathWeight / 255.0f);
            }

            // ROADS ARE WORTH MORE
            if (collision.flags & COLLISION_NAV_FLAG_ROAD) {
                pathWeights[i] *= 2.0f;
            }
        }

        // Check neighbors
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            const int i = dir - 1;
            const ui32v2& nextPoint = nextPoints[i];
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
        return nullptr;
    }

    // Generate the path by reverse iterating from the goal
    AStarNode& goalNode = getNodeLookup(goalNodeIndex);

    std::unique_ptr<Path> path = std::make_unique<Path>();
    ui32 pathSize = 0;
    {
        AStarNode* node = &goalNode;
        ui32v2 worldPoint = nodeIndexToWorldPos(goalNodeIndex, bottomLeftPoint);

        // Find out the path size and cache the points
        while (node != &startNode) {
            sPathPointBuffer[pathSize++] = worldPoint;
            const i32v2& offsetToParent = NODE_OFFSETS[node->parentDir];
            worldPoint = ui32v2((i32)worldPoint.x + offsetToParent.x, (i32)worldPoint.y + offsetToParent.y);
            node = &getNodeLookup(worldPoint, bottomLeftPoint);
        }
    }

    // TODO: Path memory pool
    path->points = std::make_unique<PathPoint[]>(pathSize);
    path->numPoints = pathSize;
    // Copy the path
    memcpy(path->points.get(), sPathPointBuffer, pathSize * sizeof(PathPoint));

    std::cout << "Generated path in " << timer.stop() << " ms with " << TOTAL << " nodes checked\n";
    path->finishedGenerating = true;
    return path;
}


void coarseAstarEdgePropagate(const World& world, const NavNode* navNode, CoarseAstarNodeID& totalAstarNodes, CoarseAStarNode* astarNodes, const ui32v2& goal, std::set<std::pair<f32, CoarseAstarNodeID>>& openList, CoarseAstarNodeID parentId, f32 prevG, ui32v2 parentPos) {
    ui32v2 chunkWorldPos = ui32v2(navNode->chunk.getWorldPos());
    for (auto& edge : navNode->edges) {
        ui32v2 position = ui32v2(chunkWorldPos.x + edge.start.getX(), chunkWorldPos.y + edge.start.getY());
        // Offset into next cell
        position = ui32v2(i32v2(position) + CARTESIAN_NORMALS[enum_cast(edge.dir)]);
        // Offset to center of edge
        position += ui32v2(f32v2(CARTESIAN_EDGE_DIRS[enum_cast(edge.dir)]) * (f32)edge.length * 0.5f);
        const NavNode* nextNode = world.tryGetNavNodeAtWorldPos(position);
        auto&& closedIt = sCoarseClosedList.find(nextNode);
        if (closedIt != sCoarseClosedList.end()) {
            // TODO: Update G if better?
            continue;
        }
        sCoarseClosedList.insert(nextNode);

        CoarseAstarNodeID newId = totalAstarNodes++;
        CoarseAStarNode& node = astarNodes[newId];
        node.position = position;
        node.g = prevG + glm::length(f32v2(node.position) - f32v2(parentPos));
        node.h = getEuclideanHeuristicAtPosition(node.position, goal);
        DebugRenderer::drawLineBetweenPoints(f32v2(node.position), f32v2(parentPos), color4(((int)node.g % 255) / 255.0f, ((int)node.h % 255) / 255.0f, 1.0f, 0.2f), 400);
        node.parentIndex = parentId;
        openList.insert(std::make_pair(node.getScore(), newId));
    } 
}

std::unique_ptr<CoarsePath> PathFinder::generateCoarsePathSynchronous(const World& world, const ui32v2& start, const ui32v2& goal)
{
    PreciseTimer timer;
    std::set<std::pair<f32, CoarseAstarNodeID>> openList;
    std::unique_ptr<CoarsePath> rvPath;

    // We pathfind backwards
    const NavNode* startNode = world.tryGetNavNodeAtWorldPos(goal);
    if (!startNode) {
        pError("Error: Failed to find coarse path due to invalid start\n");
    }
    const NavNode* endNode = world.tryGetNavNodeAtWorldPos(start);
    if (!endNode) {
        pError("Error: Failed to find coarse path due to invalid end\n");
    }

    // Case where we are in the same node, just return the goal
    if (startNode == endNode) {
        rvPath = std::make_unique<CoarsePath>();
        rvPath->points = std::make_unique<PathPoint[]>(1);
        rvPath->points[0] = goal;
        rvPath->numPoints = 1;
        rvPath->finishedGenerating = true;
        return rvPath;
    }

    // A* pathfind through the coarse graph
    // TODO: non arbitrary reserve
    sCoarseClosedList.clear();
    sCoarseClosedList.reserve(MAXIMUM_COARSE_NODES); // TODO: Move this to an init?
    sCoarseClosedList.insert(nullptr); // So we dont need explicit null check later

    CoarseAstarNodeID totalAstarNodes = 0;
    CoarseAstarNodeID id;
    bool foundGoal = false;

    // TODO: Race conditions with the navgraph generator thread?
    // Add all first edges to the open and closed lists
    sCoarseClosedList.insert(startNode);
    coarseAstarEdgePropagate(world, startNode, totalAstarNodes, sCoarseAstarNodes, start, openList, INVALID_COARSE_NODE_PARENT, 0.0f, goal);
    // Do the A*
    while (openList.size() && totalAstarNodes < MAXIMUM_COARSE_NODES - 256) {
        auto&& it = openList.begin();
        id = it->second;
        openList.erase(it);
        CoarseAStarNode& astarNode = sCoarseAstarNodes[id];
        const NavNode* navNode = world.tryGetNavNodeAtWorldPos(astarNode.position);
        if (navNode == endNode) {
            foundGoal = true;
            break;
        }

        //DebugRenderer::drawFilledQuad(astarNode.position, f32v2(1.1f), color4(1.0f, 1.0f, 1.0f, 0.5f), i, 0);
        
        coarseAstarEdgePropagate(world, navNode, totalAstarNodes, sCoarseAstarNodes, start, openList, id, astarNode.g, astarNode.position);
    }

#if PATH_DEBUG == 1
    DebugRenderer::drawFilledQuad(start, f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.2f), 200, 0);
    DebugRenderer::drawFilledQuad(goal, f32v2(1.0f), color4(1.0f, 0.0f, 0.0f, 0.2f), 200, 0);
#endif

    if (!foundGoal) {
        return nullptr;
    }

    rvPath = std::make_unique<CoarsePath>();
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
    if (sPathPointBuffer[pathSize - 1] != goal) {
        sPathPointBuffer[pathSize++] = goal;
    }

    // TODO: Path memory pool
    rvPath->points = std::make_unique<PathPoint[]>(pathSize);
    rvPath->numPoints = pathSize;
    // Copy the path
    memcpy(rvPath->points.get(), sPathPointBuffer, pathSize * sizeof(PathPoint));

    for (ui32 i = 1; i < rvPath->numPoints; ++i) {
        DebugRenderer::drawLineBetweenPoints(f32v2(rvPath->points[i - 1]), f32v2(rvPath->points[i]), color4(1.0f, 1.0f, 0.0f, 0.5f), 400);
    }

        // Back propagation
    std::cout << "Coarse path found in " << timer.stop() << "ms with " << totalAstarNodes << " total nodes checked\n";
    return rvPath;
}
