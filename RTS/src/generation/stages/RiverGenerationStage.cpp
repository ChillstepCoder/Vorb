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

    i16v2 currentNode = path.startPoint;
    f32 currentHeight = mHeightGrid->getHeightAtVert(currentNode);
    allNodes[currentNode] = NodeInfo(currentNode, 0, currentHeight);

    size_t numChecks = 0;
    while (numChecks < MAX_CHECKS) {
        if (currentHeight < 0.0f) {
            foundGoal = true;
            break;
        }

        std::array<i16v2, 4> neighbors = {
            currentNode + i16v2(0, -1), // Down
            currentNode + i16v2(-1, 0), // Left
            currentNode + i16v2(1, 0),  // Right
            currentNode + i16v2(0, 1)   // Up
        };

        bool moved = false;
        for (const auto& neighbor : neighbors) {
            f32 neighborHeight = mHeightGrid->getHeightAtVert(neighbor);
            auto it = allNodes.find(neighbor);
            int newDepth = allNodes[currentNode].depth + 1;

            if (it == allNodes.end()) {
                // Unvisited node
                allNodes[neighbor] = NodeInfo(currentNode, newDepth, neighborHeight);
                currentNode = neighbor;
                currentHeight = neighborHeight;
                moved = true;
                break;
            }
            else if (it->second.depth > newDepth) {
                // Visited node with higher depth, reroute to this path
                it->second.backPointer = currentNode;
                it->second.depth = newDepth;
            }
        }

        if (!moved) {
            // If no move was made, backtrack
            const auto& backPointer = allNodes[currentNode].backPointer;
            if (currentNode == backPointer) {
                // No further backtracking possible
                break;
            }
            currentNode = backPointer;
            currentHeight = allNodes[currentNode].height;
        }

        numChecks++;
    }

    // Building the final path
    if (foundGoal) {
        for (i16v2 node = currentNode; node != path.startPoint; node = allNodes[node].backPointer) {
            path.points.push_back(node);
        }
        path.points.push_back(path.startPoint);
        std::reverse(path.points.begin(), path.points.end());
        LOG_CRITICAL("VALID {} {}", path.points.size(), numChecks);
    }
    else {
        path.points.clear();
        LOG_CRITICAL("INVALID {}", numChecks);
    }


}
