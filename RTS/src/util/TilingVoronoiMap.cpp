#include "stdafx.h"
#include "TilingVoronoiMap.h"

#include <random>

#define DEBUG_PRINT_VORONOI_MAP 0

bool isValidPoint(const i32v2& point, float minDist, const std::vector<i32v2>& points) {
    for (const auto& p : points) {
        const int dx = p.x - point.x;
        const int dy = p.y - point.y;
        if (SQ(dx) + SQ(dy) < SQ(minDist)) {
            return false; // Point is too close to another
        }
    }
    return true;
}


std::vector<i32v2> generatePoissonPoints(float minDist, int maxAttempts, ui32 width, std::mt19937& rng) {
    std::uniform_real_distribution<float> normDist(0, 1.0f);
    std::uniform_real_distribution<float> dist(0, width);
    std::vector<i32v2> samplePoints;
    std::vector<i32v2> activePoints;

    // Initial random point
    const i32v2 p0 = i32v2(dist(rng), dist(rng));
    activePoints.push_back(p0);
    samplePoints.push_back(p0);

    while (!activePoints.empty()) {
        const int random_index = std::uniform_int_distribution<int>(0, activePoints.size() - 1)(rng);
        const i32v2 point = activePoints[random_index];

        bool found = false;
        for (int i = 0; i < maxAttempts; ++i) {
            float angle = std::uniform_real_distribution<float>(0, 2 * M_PI)(rng);
            float radius = std::uniform_real_distribution<float>(minDist, 2 * minDist)(rng);
            i32v2 newPoint = i32v2(
                static_cast<int>(point.x + radius * cos(angle)),
                static_cast<int>(point.y + radius * sin(angle))
            );

            // Check if point is within bounds and does not collide
            if (newPoint.x >= 0 && newPoint.x < width && newPoint.y >= 0 && newPoint.y < width && isValidPoint(newPoint, minDist, samplePoints)) {
                activePoints.push_back(newPoint);
                samplePoints.push_back(newPoint);
                found = true;
                break;
            }
        }
        if (!found) {
            activePoints[random_index] = activePoints.back();
            activePoints.pop_back();
        }
    }

    samplePoints.shrink_to_fit();
    return samplePoints;
}


TilingVoronoiMap::TilingVoronoiMap(ui32 width, f32 minDist) : mWidth(width) {

    mVoronoiCellLookup.resize(SQ(mWidth));

    // Generate voronoi cells centers using poisson disk sampling
    // Points are generated between [0, width] in both x and y
    // Poisson Disk Sampling parameters
    const ui32 maxAttempts = 30;
    std::mt19937 rng((3305003081u * width) * minDist); // Big prime seed

    // Generate Voronoi cell centers
    mVoronoiCellCenters = generatePoissonPoints(minDist, maxAttempts, mWidth, rng);

    for (ui32 y = 0; y < mWidth; ++y) {
        for (ui32 x = 0; x < mWidth; ++x) {
            f32 minDistSq = FLT_MAX;
            ui32 closestIndex = 0;
            for (ui32 i = 0; i < mVoronoiCellCenters.size(); ++i) {
                const i32v2& center = mVoronoiCellCenters[i];

                // Calculate wrapped distance
                f32 distX = glm::min(glm::min(glm::abs(center.x - x), glm::abs((center.x - mWidth) - x)), glm::abs((center.x + mWidth) - x));
                f32 distY = glm::min(glm::min(glm::abs(center.y - y), glm::abs((center.y - mWidth) - y)), glm::abs((center.y + mWidth) - y));
                const f32 distSq = distX * distX + distY * distY;

                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    closestIndex = i;
                }
            }
            mVoronoiCellLookup[y * mWidth + x] = closestIndex;
        }
    }

    if constexpr (DEBUG_PRINT_VORONOI_MAP) {
        LOG_DEBUG("TilingVoronoiMap Generated {} voronoi cell centers", mVoronoiCellCenters.size());
        nString grid;
        grid.resize(SQ(mWidth), '.');
        LOG_DEBUG("\n CELL CENTERS");
        int i = 0;
        for (i32v2 cellCenter : mVoronoiCellCenters) {
            grid[cellCenter.y * mWidth + cellCenter.x] = '0' + i;
            ++i;
        }
        nString row;
        row.resize(mWidth);
        for (ui32 y = 0; y < mWidth; ++y) {
            memcpy(row.data(), grid.data() + y * mWidth, mWidth);
            LOG_DEBUG("   {}", row);
        }

        LOG_DEBUG("\n TILES");
        for (ui32 y = 0; y < mWidth; ++y) {
            for (ui32 x = 0; x < mWidth; ++x) {
                row[x] = mVoronoiCellLookup[y * mWidth + x] + '0';
            }
            LOG_DEBUG("   {}", row);
        }
        LOG_DEBUG("Done");
    }
}

i32v2 TilingVoronoiMap::getVoronoiPointAtTile(i32v2 tilePosWorld, float voronoiScale) {
    const i32 scaledWidth = (i32)round((i32)mWidth * voronoiScale);
    const i32 xOff = tilePosWorld.x % scaledWidth;
    const i32 yOff = tilePosWorld.y % scaledWidth;
    const i32 xPosRel = (i32)(((f32)xOff / scaledWidth) * (f32)mWidth);
    const i32 yPosRel = (i32)(((f32)yOff / scaledWidth) * (f32)mWidth);
    assert(xPosRel >= 0 && xPosRel < mWidth);
    assert(yPosRel >= 0 && yPosRel < mWidth);
    return mVoronoiCellCenters[mVoronoiCellLookup[yPosRel * mWidth + xPosRel]];
}