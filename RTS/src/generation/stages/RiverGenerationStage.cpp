#include "stdafx.h"
#include "RiverGenerationStage.h"

#include "world/IHeightmapGrid.h"
#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"
#include "generation/WorldGenerationBlackboard.h"

#include <stack>
#include <random>

void RiverGenerationStage::begin()
{
    std::vector<i32v2> riverStarts;
    riverStarts.reserve(mBlackboard.mPeakPositions.size());
    for (i32v2 peakPos : mBlackboard.mPeakPositions) {
        if (peakPos.x > -1) {
            riverStarts.emplace_back(peakPos);
        }
    }

    if (riverStarts.size() > mGenerationData.mDesiredRiverCount) {
        auto rng = std::default_random_engine{};
        rng.seed((int)(mWorldSeed * 100.0f));
        std::shuffle(std::begin(riverStarts), std::end(riverStarts), rng);

        // Chop off the end
        riverStarts.resize(mGenerationData.mDesiredRiverCount);
    }

    mBlackboard.mRiverPaths.resize(riverStarts.size());
    const ui32 genID = sGenerationUID;

    // Generate all river paths
    for (size_t i = 0; i < riverStarts.size(); ++i) {
        mBlackboard.mRiverPaths[i].startPoint = riverStarts[i];
        Services::Threadpool::ref().addTask([this, i, genID]() {
            // Check for interrupt
            if (genID != sGenerationUID) {
                return;
            }

            generateRiverPath(i);

            ++mNumFinishedRiverPaths;
        },
        nullptr);
    }
}

bool RiverGenerationStage::update() {
    if (mNumFinishedRiverPaths == mBlackboard.mRiverPaths.size()) {

        size_t i = 0;
        for (auto&& r : mBlackboard.mRiverPaths) {
            if (!r.points.empty()) {
                ++i;
            }
        }
        mBlackboard.mRiversDone = true;
        LOG_CRITICAL("Generated {} valid rivers!", i);

        return true;
    }
    return false;
}

struct NodeInfo {
    NodeInfo() = default;
    NodeInfo(const i16v2& backPointer, int depth, f32 height)
        : backPointer(backPointer), depth(depth), height(height) {}


    i16v2 backPointer;
    int depth;
    f32 height;
};

void RiverGenerationStage::generateRiverPath(size_t riverIndex) {
    constexpr size_t MAX_CHECKS = 16384;

    std::unordered_map<i16v2, NodeInfo> allNodes;
    allNodes.reserve(16384);

    RiverPath& path = mBlackboard.mRiverPaths[riverIndex];
    path.points.reserve(1024);

    bool foundGoal = false;

    constexpr i16 GRANULARITY = 8;

    i16v2 currentNode = (path.startPoint / GRANULARITY) * GRANULARITY;
    const i16v2 startNode = currentNode;
    f32 currentHeight = mHeightGrid->getHeightAtVert(currentNode);
    allNodes[currentNode] = NodeInfo(currentNode, 0, currentHeight);

    size_t numChecks = 0;
    while (numChecks < MAX_CHECKS) {
        if (currentHeight < -1.0f) [[unlikely]] {
            foundGoal = true;
            break;
        }

        NodeInfo& currentNodeInfo = allNodes[currentNode];

        const i16v2 down = currentNode + i16v2(0, -GRANULARITY);
        const i16v2 left = currentNode + i16v2(-GRANULARITY, 0);
        const i16v2 right = currentNode + i16v2(GRANULARITY, 0);
        const i16v2 up = currentNode + i16v2(0, GRANULARITY);
        std::array<std::pair<i16v2, f32>, 4> neighbors = {
            std::pair{down, mHeightGrid->getHeightAtVert(down)}, // Down
            std::pair{left, mHeightGrid->getHeightAtVert(left)}, // Left
            std::pair{right,  mHeightGrid->getHeightAtVert(right)},  // Right
            std::pair{up,  mHeightGrid->getHeightAtVert(up)}   // Up
        };

        f32 bestChildHeight = FLT_MAX;
        int bestChildIndex = -1;

        // First check if any neighbors should be our new parent or greedy child
        for (int i = 0; i < 4; ++i) {
            const auto& [neighborPos, neighborHeight] = neighbors[i];
            auto it = allNodes.find(neighborPos);

            if (it == allNodes.end()) {
                if (neighborHeight < bestChildHeight) {
                    bestChildHeight = neighborHeight;
                    bestChildIndex = i;
                }
            }
            else if (it->second.depth < currentNodeInfo.depth - 1) {
                // Shorter route found
                currentNodeInfo.depth = it->second.depth + 1;
                currentNodeInfo.backPointer = neighborPos;
            }
        }

        // Greedily select best child
        if (bestChildIndex != -1) {
            const int newDepth = currentNodeInfo.depth + 1;
            auto& [neighborPos, neighborHeight] = neighbors[bestChildIndex];
            // Unvisited node
            allNodes[neighborPos] = NodeInfo(currentNode, newDepth, neighborHeight);
            currentNode = neighborPos;
            currentHeight = neighborHeight;
        }
        else {
            // If no move was made, backtrack
            const auto& backPointer = allNodes[currentNode].backPointer;
            if (currentNode == backPointer) {
                // No further backtracking possible
                break;
            }
            currentNode = backPointer;
            currentHeight = allNodes[currentNode].height;
        }

        ++numChecks;
    }

    // LOGGING
    constexpr bool LOG_ENABLED = false;

    // Building the final path
    if (foundGoal) {
        for (i16v2 node = currentNode; node != startNode; node = allNodes[node].backPointer) {
            path.points.push_back(node);
        }
        path.points.push_back(startNode);
        path.isValid = true;
       if constexpr (LOG_ENABLED) LOG_CRITICAL("VALID {} {}", path.points.size(), numChecks);
    }
    else {
        for (i16v2 node = currentNode; node != startNode; node = allNodes[node].backPointer) {
            path.points.push_back(node);
        }
        path.points.push_back(startNode);
        path.isValid = false;
        if constexpr (LOG_ENABLED) LOG_CRITICAL("INVALID {} ", numChecks);
    }
    // TODO: REMOVE
    path.visited.reserve(allNodes.size());
    for (auto& it : allNodes) {
        path.visited.push_back(it.first);
    }
}

