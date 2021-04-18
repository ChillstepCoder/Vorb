#include "stdafx.h"
#include "PathFinder.h"

#include "World.h"

#include "DebugRenderer.h"
#define PATH_DEBUG 1

constexpr ui32 MAX_PATH_LENGTH = 255;
constexpr ui8 INVALID_PARENT = 0;

constexpr ui32 MAX_OPEN_LIST_SIZE = MAX_PATH_LENGTH * 8;

struct NodeListElement {
    i32 nodeIndex;
    ui16 score;
};

class NodeList {
public:
    void add(i32 index, ui16 score) {
        assert(mSize < MAX_OPEN_LIST_SIZE);
        mNodes[mEnd++] = { index, score };
        if (mEnd >= MAX_OPEN_LIST_SIZE) mEnd = 0;
        ++mSize;
    }

    // TODO: I think we could have a sorted array that is easy to sort by storing the counts of each node weight
    // and having an array of data with entries for each integer node score, then we can build sort offsets for each
    // group, and increment a secondary offset array (is this faster than linear search?)
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

constexpr ui32 HALF_LOOKUP_LIST_WIDTH = MAX_PATH_LENGTH;
constexpr ui32 LOOKUP_LIST_WIDTH = HALF_LOOKUP_LIST_WIDTH * 2;
constexpr ui32 LOOKUP_LIST_SIZE = SQ(LOOKUP_LIST_WIDTH);
// No allocations baby
thread_local AStarNode sNodes[LOOKUP_LIST_SIZE] = {};
thread_local PathPoint sPathPointBuffer[4096];

#define MANHATTAN 1

// http://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html
f32 getDiagonalHeuristicAtPosition(const ui32v2& node, const ui32v2& goal) {

    constexpr f32 D = 1;
    const f32 D2 = 1;

#if MANHATTAN == 1
    i32 dx = abs((i32)node.x - (i32)goal.x);
    i32 dy = abs((i32)node.y - (i32)goal.y);
    return D * (dx + dy);
#else
    // When D = 1 and D2 = 1, this is called the Chebyshev distance.
    // When D = 1 and D2 = sqrt(2), this is called the octile distance.
    // TODO: Truncation error for large integers
    f32 dx = abs((f32)node.x - (f32)goal.x);
    f32 dy = abs((f32)node.y - (f32)goal.y);
    return D * (dx + dy) + (D2 - 2 * D) * vmath::min(dx, dy);
#endif
}

i32 getNodeIndex(const i32v2 node, const i32v2 bottomLeft) {
    assert(node.x >= bottomLeft.x && node.y >= bottomLeft.y);
    const i32v2 offset = node - bottomLeft;
    return offset.y * LOOKUP_LIST_WIDTH + offset.x;
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

// https://github.com/daancode/a-star/blob/master/source/AStar.cpp
std::unique_ptr<Path> PathFinder::generatePathSynchronous(const World& world, const ui32v2& start, const ui32v2& goal) {
    // TODO: Profiling
    PreciseTimer timer;

    ui32 debugCount = 0;

    // Make sure we can fit in nodes array
    ui32 dx = (ui32)abs((i32)start.x - (i32)goal.x);
    ui32 dy = (ui32)abs((i32)start.y - (i32)goal.y);
    if (dx >= HALF_LOOKUP_LIST_WIDTH || dy >= HALF_LOOKUP_LIST_WIDTH) {
        return nullptr;
    }
    // Clear out the nodes
    // TODO: Is this faster or slower than using a closed list?
    memset(sNodes, 0, sizeof(AStarNode) * LOOKUP_LIST_SIZE);

    // This must remain sorted
    NodeList openList;

    const ui32v2 bottomLeftPoint = start - ui32v2(HALF_LOOKUP_LIST_WIDTH, HALF_LOOKUP_LIST_WIDTH);

    // Add start node to the open list
    const i32 startNodeIndex = getNodeIndex(start, bottomLeftPoint);
    AStarNode& startNode = getNodeLookup(startNodeIndex);
    startNode.isInOpenList = true;
    startNode.g = 0;
    startNode.h = getDiagonalHeuristicAtPosition(start, goal);
    openList.add(startNodeIndex, startNode.getScore());

    const i32 goalNodeIndex = getNodeIndex(goal, bottomLeftPoint);
    bool foundGoal = false;
    while (openList.size()) {
        // Pull best node off of the open list (Linear search)
        const ui32 nodeIndex = openList.popLowestScoreNode().nodeIndex;
        if (nodeIndex == goalNodeIndex) {
            foundGoal = true;
            break;
        }

        AStarNode& node = getNodeLookup(nodeIndex);
        // Add current node to implicit closed list
        node.isInOpenList = false;
        node.isInClosedList = true;

        const ui32v2 nodePoint = nodeIndexToWorldPos(nodeIndex, bottomLeftPoint);
        
#if PATH_DEBUG == 1
        if (debugCount > 255) debugCount = 0;
        DebugRenderer::drawQuad(nodePoint, f32v2(1.0f), color4(debugCount++ / 255.0f, node.h / 128.0f, 0.0f, 0.2f), 200, 0);
#endif

        // Precompute collision weights and points for neighbors
        ui32v2 nextPoints[8];
        f32 pathWeights[8];
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            const i32v2& offset = NODE_OFFSETS[dir];
            int i = dir - 1;
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
            TileHandle tile = world.getTileHandleAtWorldPos(nextPoint);
            // TODO: CollisionMap for less lookups
            f32 weight = 1.0f;
            for (int l = 0; l < 3; ++l) {
                TileID tileId = tile.tile.layers[l];
                if (tileId != TILE_ID_NONE) {
                    const TileData tileData = TileRepository::getTileData(tileId);
                    weight *= tileData.pathWeight;
                }
            }
            pathWeights[i] = weight;
        }

        // Check neighbors
        for (int dir = NODE_DIR_DOWN_LEFT; dir <= NODE_DIR_UP_RIGHT; ++dir) {
            int i = dir - 1;
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
                // EXIT CONDITION, PATH TOO LONG
                if (nextNodeIndex == 0) {
                    return nullptr;
                }
                AStarNode& nextNode = getNodeLookup(nextPoint, bottomLeftPoint);
                const ui32 cost = node.g + MOVEMENT_COSTS[dir];
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
                        nextNode.h = getDiagonalHeuristicAtPosition(nextPoint, goal);
                        // This could be a single assignment since bitfield
                        nextNode.isInClosedList = false;
                        nextNode.isInOpenList = true;
                        openList.add(nextNodeIndex, nextNode.getScore());
                    }
                }
                else {
                    nextNode.g = cost;
                    nextNode.h = getDiagonalHeuristicAtPosition(nextPoint, goal);
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

    std::cout << "Generated path in " << timer.stop() << " ms\n";
    path->finishedGenerating = true;
    return path;
}
