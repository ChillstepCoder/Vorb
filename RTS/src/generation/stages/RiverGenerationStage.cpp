#include "stdafx.h"
#include "RiverGenerationStage.h"

#include "world/IHeightmapGrid.h"
#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"
#include "generation/WorldGenerationBlackboard.h"

#include "rendering/MaterialShaderRepository.h"
#include "resources/TextureRepository.h"

#include <tinysplinecxx.h>

#include <random>

RiverGenerationPass::~RiverGenerationPass() {
    if (sync) {
        glDeleteSync(sync);
    }
    if (segmentsBuffer) {
        glDeleteBuffers(1, &segmentsBuffer);
    }
    if (groupDataBuffer) {
        glDeleteBuffers(1, &groupDataBuffer);
    }
}

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
        });
    }
}

bool RiverGenerationStage::update() {
    if (mNumFinishedRiverPaths == mBlackboard.mRiverPaths.size() && !mFinishedGeneratingPaths) {

        size_t numValidRivers = 0;
        for (numValidRivers = 0; numValidRivers < mBlackboard.mRiverPaths.size();) {
            RiverPath& r = mBlackboard.mRiverPaths[numValidRivers];
            if (r.splinePath.empty()) {
                // Remove invalid river
                r = std::move(mBlackboard.mRiverPaths.back());
                mBlackboard.mRiverPaths.pop_back();
            }
            else {
                ++numValidRivers;
            }
        }
        LOG_DEBUG("Generated {} valid rivers paths", numValidRivers);
        mBlackboard.mRiverSplinesGenerated = true;
        mFinishedGeneratingPaths = true;

        if (mBlackboard.mRiverPaths.empty()) {
            // No rivers, no passes
            return true;
        }

        // Build generations
        // Each cell has a number of passes, based on how many different rivers influence them

        // TODO: refine reserve
        mCellPasses.reserve(mBlackboard.mRiverPaths.size() * 4);

        // Track all passes in each cell
        for (auto&& it : mBlackboard.mRiverPaths) {
            for (auto&& it2 : it.affectedLocalGroups) {
                CellPassData& passData = mCellPasses[it2.first];
                const ui32 passIndex = passData.passes.size();
                CellPassData::PassData& pass = passData.passes.emplace_back();
                pass.riverSegments = &it2.second;
                if (passIndex >= mGPUGenerations.size()) [[unlikely]] {
                    mGPUGenerations.resize(passIndex + 1);
                }
                RiverGenerationPass& generationPass = mGPUGenerations[passIndex];
                // Track where in the array we belong
                pass.groupIndex = generationPass.numLocalGroups;
                pass.segmentStart = generationPass.numSegments;
                generationPass.numSegments += it2.second.size();
                ++generationPass.numLocalGroups;
            }
        }

        // Build passes
        for (size_t passIndex = 0; passIndex < mGPUGenerations.size(); ++passIndex) {
            RiverGenerationPass& pass = mGPUGenerations[passIndex];
            pass.passIndex = passIndex;
            pass.allSegments.resize(pass.numSegments);
            pass.bufferData.resize(pass.numLocalGroups);
        }
        mGeneratingPasses = true;

        // Large copy so dont stall main thread
        const ui32 genID = sGenerationUID;
        Services::Threadpool::ref().addTask([this, genID]() {

            for (auto&& it : mCellPasses) {
                // Check for interrupt
                if (genID != sGenerationUID) [[unlikely]] {
                    return;
                }
                CellPassData& cellPassData = it.second;
                for (size_t passIndex = 0; passIndex < cellPassData.passes.size(); ++passIndex) {
                    const CellPassData::PassData& pass = cellPassData.passes[passIndex];
                    const std::vector<f32v4>* river = pass.riverSegments;
                    RiverGenerationPass& generationPass = mGPUGenerations[passIndex];
                    RiverGenerationBufferData& bufferData = generationPass.bufferData[pass.groupIndex];

                    bufferData.cellPos = it.first;
                    bufferData.segmentStartIndex = pass.segmentStart;
                    bufferData.numSegments = river->size();

                    for (ui32 i = 0; i < bufferData.numSegments; ++i) {
                        generationPass.allSegments[pass.segmentStart + i] = river->operator[](i);
                    }
                }
            }

            mGeneratingPasses = false;
        });

    }

    if (mFinishedGeneratingPaths) {

        if (mGeneratingPasses) {
            return false;
        }

        // Download height
        if (mAllGpuGenerationsFinished) {
            if (mFinishedHeightDownloads == mPendingHeightDownloads) {
                // Free unused memory
                UnorderedFlatMap<i32v2 /*vertexPosCorner*/, CellPassData>().swap(mCellPasses);
                return true;
            }
            return false;
        }
        else {

            // Update compute
            while (mNextPassIndex != mGPUGenerations.size()) {
                RiverGenerationPass& pass = mGPUGenerations[mNextPassIndex];
                if (!pass.generateStarted) {

                    const MaterialShaderDef* def = MaterialShaderRepository::get().tryGetLoadedAsset(CStrToken("river_carve"));
                    if (!def) {
                        panic("river_carve.comp was not loaded. Make sure it exists and is in assets.preload");
                    }
                    def->useCompute();
                    glUniform1ui(def->getUniform("unYStride"), mHeightGrid->getWidthPatches() * (ui32)HEIGHTMAP_VERT_WIDTH_PER_PATCH);

                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mHeightSSBO);
                    glBindImageTexture(0, mHeightTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

                    glCreateBuffers(1, &pass.segmentsBuffer);
                    glNamedBufferStorage(pass.segmentsBuffer, sizeof(f32v4) * pass.allSegments.size(), pass.allSegments.data(), 0);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, pass.segmentsBuffer);

                    glCreateBuffers(1, &pass.groupDataBuffer);
                    glNamedBufferStorage(pass.groupDataBuffer, sizeof(RiverGenerationBufferData) * pass.bufferData.size(), pass.bufferData.data(), 0);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pass.groupDataBuffer);

                    glBindTextureUnit(3, TextureRepository::get().getLoadedAsset(CStrToken("perlin_noise")).gpuTexture.getHandle());

                    constexpr i32 MAX_COMPUTE_SIZE = 65535; // Minimum as according to openGL spec

                    if (MAX_COMPUTE_SIZE < pass.bufferData.size()) {
                        panic("River compute attempted to dispatch {} groups, but max is {}", pass.bufferData.size(), MAX_COMPUTE_SIZE);
                    }
                    glDispatchCompute(pass.bufferData.size(), 1, 1);
                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    pass.generateStarted = true;
                    pass.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

                    checkGlError("RiverGenerationStage::update");

                    // We don't need this memory anymore
                    std::vector<f32v4>().swap(pass.allSegments);
                    break;
                }
                GLenum waitResult = glClientWaitSync(pass.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
                assert(pass.sync);
                if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
                    onPassFinished(pass);
                    ++mNextPassIndex;
                    if (mNextPassIndex >= mGPUGenerations.size()) {
                        mAllGpuGenerationsFinished = true;
                        return false;
                    }
                }
                else if (waitResult == GL_TIMEOUT_EXPIRED) {
                    // The GPU commands are not yet complete. Continue other tasks or loop back later.
                    return false;
                }
                else {
                    panic("Terrain generation sync failed with GL_WAIT_FAILED");
                }
            }
        }
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

std::vector<f32v2> smoothPath(const std::vector<i16v2>& points) {

    // Create a spline with degree 3 (cubic) and 2D control points
    tinyspline::BSpline spline(points.size(), 2, 3, tinyspline::BSpline::Type::Clamped);

    // Set control points
    std::vector<tinyspline::real> ctrlp = spline.controlPoints();
    for (size_t i = 0; i < points.size(); ++i) {
        ctrlp[2 * i] = (tinyspline::real)points[i].x;
        ctrlp[2 * i + 1] = (tinyspline::real)points[i].y;
    }
    spline.setControlPoints(ctrlp);

    // Generate smoothed path
    constexpr int SEGMENTS_PER_POINT = 3;
    const int numPoints = points.size() * SEGMENTS_PER_POINT;
    std::vector<f32v2> smoothedPath;
    smoothedPath.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        const float u = (float)i / (numPoints - 1); // normalize u to [0, 1]
        std::vector<tinyspline::real> result = spline.eval(u).result();
        smoothedPath.emplace_back(result[0], result[1]);
    }

    return smoothedPath;
}

void RiverGenerationStage::generateRiverPath(size_t riverIndex) {

    constexpr size_t MAX_CHECKS = 65536;

    UnorderedFlatMap<i16v2, NodeInfo> allNodes;
    allNodes.reserve(16384);

    RiverPath& path = mBlackboard.mRiverPaths[riverIndex];

    bool foundGoal = false;

    constexpr i16 GRANULARITY = 8;
    constexpr f32 GOAL_HEIGHT = -8.0f;

    i16v2 currentNode = (path.startPoint / GRANULARITY) * GRANULARITY;
    const i16v2 startNode = currentNode;
    f32 currentHeight = mHeightGrid->getHeightAtVert<false>(DTileCoord(currentNode));
    allNodes[currentNode] = NodeInfo(currentNode, 0, currentHeight);

    size_t numChecks = 0;
    while (numChecks < MAX_CHECKS) {
        if (currentHeight < GOAL_HEIGHT) [[unlikely]] {
            foundGoal = true;
            break;
        }

        NodeInfo& currentNodeInfo = allNodes[currentNode];

        const i16v2 down = currentNode + i16v2(0, -GRANULARITY);
        const i16v2 left = currentNode + i16v2(-GRANULARITY, 0);
        const i16v2 right = currentNode + i16v2(GRANULARITY, 0);
        const i16v2 up = currentNode + i16v2(0, GRANULARITY);
        std::pair<i16v2, f32> neighbors[4] = {
            std::pair{down, mHeightGrid->getHeightAtVert<false>(DTileCoord(down))}, // Down
            std::pair{left, mHeightGrid->getHeightAtVert<false>(DTileCoord(left))}, // Left
            std::pair{right,  mHeightGrid->getHeightAtVert<false>(DTileCoord(right))},  // Right
            std::pair{up,  mHeightGrid->getHeightAtVert<false>(DTileCoord(up))}   // Up
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

    // Config
    constexpr bool LOG_ENABLED = false;
    constexpr bool TRACK_ALL_VISITED = false;

    // Building the final path
    {
        std::vector<i16v2> points;
        if (foundGoal) {
            points.reserve(allNodes[currentNode].depth);
            for (i16v2 node = currentNode; node != startNode; node = allNodes[node].backPointer) {
                int preturbX = (((int)Random::getThreadSafe(node.x, node.y) % 7)) - 3;
                int preturbY = (((int)Random::getThreadSafe(node.y, node.x) % 7)) - 3;
                points.emplace_back(node.x + preturbX, node.y + preturbY);
            }
            points.push_back(startNode);
            path.isValid = true;
            if constexpr (LOG_ENABLED) LOG_CRITICAL("VALID {} {}", points.size(), numChecks);

            // Build spline points
            assert(points.size());
            path.splinePath = smoothPath(points);
        }
        else {
            points.reserve(allNodes[currentNode].depth);
            for (i16v2 node = currentNode; node != startNode; node = allNodes[node].backPointer) {
                points.push_back(node);
            }
            points.push_back(startNode);
            path.isValid = false;
            if constexpr (LOG_ENABLED) LOG_CRITICAL("INVALID {} ", numChecks);
        }
    }

    // Build local group data for compute shader
    if (path.splinePath.size()) {
        for (size_t i = 0; i < path.splinePath.size() - 1; ++i) {
            const f32v2 point = path.splinePath[i];
            const f32v2 nextPoint = path.splinePath[i + 1];

            // Copy our segment to all 9 nearest cells
            const i32v2 middle = (((i32v2)point) / RIVER_CARVE_LOCAL_GROUP_SIZE) * RIVER_CARVE_LOCAL_GROUP_SIZE;
            // Round the 4 corners to local group positions
            const i32v2 cellPositions[9] = {
                middle + i32v2(-RIVER_CARVE_LOCAL_GROUP_SIZE, -RIVER_CARVE_LOCAL_GROUP_SIZE), // BL
                middle + i32v2(0, -RIVER_CARVE_LOCAL_GROUP_SIZE), // B
                middle + i32v2(RIVER_CARVE_LOCAL_GROUP_SIZE, -RIVER_CARVE_LOCAL_GROUP_SIZE), // BR
                middle + i32v2(-RIVER_CARVE_LOCAL_GROUP_SIZE, 0), // L
                middle, // M
                middle + i32v2(RIVER_CARVE_LOCAL_GROUP_SIZE, 0), // R
                middle + i32v2(-RIVER_CARVE_LOCAL_GROUP_SIZE, RIVER_CARVE_LOCAL_GROUP_SIZE), // TL
                middle + i32v2(0, RIVER_CARVE_LOCAL_GROUP_SIZE), // T
                middle + i32v2(RIVER_CARVE_LOCAL_GROUP_SIZE, RIVER_CARVE_LOCAL_GROUP_SIZE) // TR
            };

            for (int j = 0; j < 9; ++j) {
                std::vector<f32v4>& segments = path.affectedLocalGroups[cellPositions[j]];
                segments.emplace_back(point.x, point.y, nextPoint.x, nextPoint.y);
            }
        }
    }

    if constexpr (LOG_ENABLED) {
        int segmentCount = 0;
        for (auto&& it : path.affectedLocalGroups) {
            segmentCount += it.second.size();
        }
        LOG_CRITICAL("Groups: {} Total Segments: {}", path.affectedLocalGroups.size(), segmentCount);
    }

    if constexpr (TRACK_ALL_VISITED) {
        path.visited.reserve(allNodes.size());
        for (auto& it : allNodes) {
            path.visited.push_back(it.first);
        }
    }
}

void RiverGenerationStage::onPassFinished(RiverGenerationPass& pass) {
    glDeleteSync(pass.sync);
    pass.sync = 0;
    glDeleteBuffers(1, &pass.segmentsBuffer);
    pass.segmentsBuffer = 0;
    glDeleteBuffers(1, &pass.groupDataBuffer);
    pass.groupDataBuffer = 0;

    // One task per patch
    const ui32 genID = sGenerationUID;
    for (int p = 0; p < pass.bufferData.size(); ++p) {
        const RiverGenerationBufferData& data = pass.bufferData[p];
        assert(pass.passIndex <= mCellPasses[data.cellPos].passes.size() - 1);
        if (pass.passIndex == mCellPasses[data.cellPos].passes.size() - 1) {
            ++mPendingHeightDownloads;
            // This cell has no more pending generations so we can initiate the download
            Services::Threadpool::ref().addTask([this, cellPos = data.cellPos, genID]() {
                // Check for interrupt
                if (genID != sGenerationUID) {
                    return;
                }
                const ui32v2 heightmapXY = cellPos / ui32(HEIGHTMAP_VERT_WIDTH_PER_PATCH);
                // Set height data
                const HeightmapPatchID patchId = heightmapXY.y * mHeightGrid->getWidthPatches() + heightmapXY.x;
                const i32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchId);
                const ui32 totalWidthVerts = mHeightGrid->getWidthPatches() * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                const i32v2 terrainRootXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchId);
                HeightmapPatch& patch = mHeightGrid->getPatchForGeneration(patchId);
                f32AABB3& aabb = patch.aabb;
               
                // Recalculate AABB
                f32 minZ = aabb.pos.z;
                f32 maxZ = aabb.pos.z + aabb.dims.z;
                {
                    const i32 rootVert = terrainRootXY.y * totalWidthVerts * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + terrainRootXY.x * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                    for (i32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
                        const i32 yStrideSource = y * totalWidthVerts;
                        const i32 yStrideTarget = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
                        for (i32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                            const i32 targetVert = yStrideTarget + x;
                            const i32 sourceVert = rootVert + yStrideSource + x;
                            const float height = mMappedHeights[sourceVert];
                            patch.setHeightAtNoClamp(targetVert, mMappedHeights[sourceVert]);
                        }
                    }
                }
                aabb.pos.z = minZ;
                aabb.dims.z = maxZ - minZ;
                patch.boundingSphere = boundingSphereFromAABB(aabb);

                ++mFinishedHeightDownloads;
            });
        }
    }
}