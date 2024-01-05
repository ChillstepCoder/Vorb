#include "stdafx.h"
#include "TilingVoronoiMap.h"

#include <random>

bool isValidPoint(const i32v2& point, float minDist, std::span<i32v2> points) {
    for (const auto& p : points) {
        const int dx = p.x - point.y;
        const int dy = p.x - point.y;
        if (SQ(dx) + SQ(dy) < SQ(minDist)) {
            return false; // Point is too close to another
        }
    }
    return true;
}

TilingVoronoiMap::TilingVoronoiMap(ui32 width, ui32 numCells) : mWidth(width) {
    mVoronoiCellLookup.resize(SQ(mWidth));
    mVoronoiCellCenters.resize(numCells);

    // Generate voronoi cells centers using poisson disk sampling
    // Points are generated between [0, width] in both x and y
    // Poisson Disk Sampling parameters
    float minDist = std::sqrt((float)(SQ(mWidth) / numCells));
    std::mt19937 rng((3305003081u * width) * numCells); // Big prime seed
    std::uniform_int_distribution<int> dist(0, mWidth - 1);

    // Generate Voronoi cell centers
    for (ui32 i = 0; i < numCells; ++i) {
        i32v2 newPoint;
        bool collision;
        do {
            collision = false;
            newPoint = i32v2(dist(rng), dist(rng));

            // Check for collision with existing points
            if (!isValidPoint(newPoint, minDist, std::span<i32v2>(mVoronoiCellCenters.data(), i))) {
                collision = true;
            }
        } while (collision);

        mVoronoiCellCenters[i] = newPoint;
    }
}
