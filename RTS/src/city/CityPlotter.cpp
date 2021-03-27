#include "stdafx.h"

#include "CityPlotter.h"
#include "city/City.h"

#include "Random.h"

// c = center, r = halfwidth
bool testAABBAABB_SIMD(const ui32v4& a, const ui32v4& b)
{
    // SIMD optimized AABB-AABB test
    // Optimized by removing conditional branches
    const i64 cxa = a.x + a.z / 2;
    const i64 cya = a.y + a.w / 2;
    const i64 cxb = b.x + b.z / 2;
    const i64 cyb = b.y + b.w / 2;
    // -1 to check if within
    const bool x = std::fabs((i64)cxa - (i64)cxb) <= (((i64)a.z + (i64)b.z) / 2) - 1;
    const bool y = std::fabs((i64)cya - (i64)cyb) <= (((i64)a.w + (i64)b.w) / 2) - 1;

    return x && y;
}

CityPlotter::CityPlotter(City& city) :
    mCity(city)
{
}

void CityPlotter::initAsTier(int tier)
{
    upgradeTier(); // Tier 0
    for (int i = 1; i <= tier; ++i) {
        upgradeTier();
    }
}

// City district evolution:
// Tier 00 +Rural +Outpost(Optional in high threat area for colonies)
// Tier 01 +Residential +Farming // Residential includes constable/sheriff 
// Tier 02 +Commercial (Town Square, move city center) +Harbor(if ocean)
// Diverge here based on threat level?
// Tier 03 +Government +Military +Industrial
// Tier 04 +Residential +Commercial +Bathhouse
void CityPlotter::upgradeTier()
{
    // TODO: Variable
    constexpr ui32 DISTRICT_SIZE = 128;
    // First tier
    if (mCurrentTier == UINT_MAX) {
        mCurrentTier = 0;
        addDistrict(DistrictTypes::Rural, nullptr, DISTRICT_SIZE);
    }
    else {
        ++mCurrentTier;
        // TODO: Data driven
        switch (mCurrentTier) {
            case 1: {
                CityDistrict* ruralParent = mDistricts.back().get();
                addDistrict(DistrictTypes::Farming, ruralParent, DISTRICT_SIZE);
                addDistrict(DistrictTypes::Residential, ruralParent, DISTRICT_SIZE);
                break;
            }
            case 2: {
                CityDistrict* residentialParent = mDistricts.back().get();
                addDistrict(DistrictTypes::Commercial, residentialParent, DISTRICT_SIZE);
                //addDistrict(DistrictTypes::Harbor, residentialParent);
                break;
            }
            case 3: {
                CityDistrict* commercialParent = mDistricts.back().get();
                addDistrict(DistrictTypes::Government, commercialParent, DISTRICT_SIZE);
                addDistrict(DistrictTypes::Military, mDistricts.back().get(), DISTRICT_SIZE); // Parent to government
                addDistrict(DistrictTypes::Industrial, commercialParent, DISTRICT_SIZE);
                break;
            }
            case 4: {
                CityDistrict* industrialParent = mDistricts.back().get();
                addDistrict(DistrictTypes::Residential, industrialParent, DISTRICT_SIZE);
                addDistrict(DistrictTypes::Commercial, mDistricts.back().get(), DISTRICT_SIZE); // Parent to Residential
                break;
            }
            case 5: {
                // TODO: More tiers
                assert(false);
            }
        }
    }
}

i32v2 getDistrictXY(int districtIndex) {
    return i32v2(districtIndex % DISTRICT_GRID_WIDTH, districtIndex / DISTRICT_GRID_WIDTH);
}

CityDistrict* CityPlotter::addDistrict(DistrictTypes type, CityDistrict* parent, ui32 size)
{
    std::unique_ptr<CityDistrict> newDistrict = std::make_unique<CityDistrict>();
    newDistrict->type = type;

    // TODO: Rectangles?
    // Dimensions
    newDistrict->aabb.z = size;
    newDistrict->aabb.w = size;

    // Check if we have parent or we are root
    if (!parent) {
        // Initialize AABB
        const ui32v2& cityCenter = mCity.getCityCenterWorldPos();
        newDistrict->aabb.x = cityCenter.x - newDistrict->aabb.z / 2;
        newDistrict->aabb.y = cityCenter.y - newDistrict->aabb.w / 2;

        newDistrict->districtGridIndex = DISTRICT_GRID_SIZE / 2;
        mDistrictGrid[newDistrict->districtGridIndex] = newDistrict.get();
    }
    else {
        assert(parent->numChildren < 4);
        // We are child
        newDistrict->parent = parent;

        // Pick random free child spot
        int randDirection = Random::xorshf96() % 4;
        const i32v2 parentCoords = getDistrictXY(parent->districtGridIndex);
        i32v2 newCoords;
        bool didFail = false;
        for (int i = 0;; ++i) {
            if (i == 4) {
                // No valid child
                didFail = true;
                break;
            }
            if (!parent->children[randDirection] && randDirection != enum_cast(parent->parentDirection)) {
                // Possibly valid child, check if it fits in the grid

                newCoords = parentCoords + CARTESIAN_OFFSETS[randDirection];
                if (!mDistrictGrid[newCoords.y * DISTRICT_GRID_WIDTH + newCoords.x]) {
                    // Valid!
                    break;
                }

            }

            ++randDirection;
            if (randDirection == 4) {
                randDirection = 0;
            }
        }

        if (didFail) {
            // TODO: Pick a new valid spot for district
            return nullptr;
        }
        else {
            parent->children[randDirection] = newDistrict.get();
            newDistrict->parentDirection = CARTESIAN_OPPOSITES[randDirection];
            newDistrict->districtGridIndex = newCoords.y * DISTRICT_GRID_WIDTH + newCoords.x;
            mDistrictGrid[newDistrict->districtGridIndex] = newDistrict.get();
        }
        
        // Set AABB using neighbor AABB of parent
        // TODO: Center on roads? Collide with other districts
        switch (randDirection) {
            case enum_cast(Cartesian::DOWN):
                newDistrict->aabb.x = parent->aabb.x;
                newDistrict->aabb.y = parent->aabb.y - newDistrict->aabb.w;
                break;
            case enum_cast(Cartesian::LEFT):
                newDistrict->aabb.x = parent->aabb.x - newDistrict->aabb.z;
                newDistrict->aabb.y = parent->aabb.y;
                break;
            case enum_cast(Cartesian::RIGHT):
                newDistrict->aabb.x = parent->aabb.x + parent->aabb.z;
                newDistrict->aabb.y = parent->aabb.y;
                break;
            case enum_cast(Cartesian::UP):
                newDistrict->aabb.x = parent->aabb.x;
                newDistrict->aabb.y = parent->aabb.y + parent->aabb.w;
                break;
        }

        ++parent->numChildren;
    }
    // Added the root plot
    const ui32 rootPlotIndex = mPlots.size();
    mPlots.emplace_back(newDistrict->aabb, 0, newDistrict.get());

    // Plot Roads
    constexpr ui32 MAIN_ROAD_WIDTH = 5;
    {
        const ui32v2 startPos(newDistrict->aabb.x, newDistrict->aabb.y + newDistrict->aabb.w / 2);
        addRoad(*newDistrict,
            startPos,
            ui32v2(startPos.x + newDistrict->aabb.z, startPos.y),
            MAIN_ROAD_WIDTH, AXIS_HORIZONTAL);
    }
    {
        const ui32v2 startPos(newDistrict->aabb.x + newDistrict->aabb.z / 2, newDistrict->aabb.y);
        addRoad(*newDistrict,
            startPos,
            ui32v2(startPos.x, startPos.y + newDistrict->aabb.w),
            MAIN_ROAD_WIDTH, AXIS_VERTICAL);
    }

    mDistricts.emplace_back(std::move(newDistrict));
    return mDistricts.back().get();
}

CityPlot* CityPlotter::addPlot(ui32v2 dims)
{
    return nullptr;
}

void CityPlotter::addRoad(CityDistrict& district, ui32v2 startPos, ui32v2 endPos, ui32 width, int axis)
{
    CityRoad road;
    road.startPos = startPos;
    road.endPos = endPos;
    road.width = width;
    if (axis == AXIS_VERTICAL) {
        assert(road.startPos.y < road.endPos.y);
        road.aabb = {
            road.startPos.x - road.width / 2,
            road.startPos.y,
            road.width,
            road.endPos.y - road.startPos.y
        };
    }
    else {
        assert(road.startPos.x < road.endPos.x);
        road.aabb = {
            road.startPos.x,
            road.startPos.y - road.width / 2,
            road.endPos.x - road.startPos.x,
            road.width
        };
    }
    district.roads.emplace_back(mCity.addRoad(road));

    // Don't do recursive splitting
    size_t stop = mPlots.size();
    for (size_t i = 0; i < stop;) {
        if (splitPlotByAABBIntersect(i, road.aabb)) {
            ++i;
        }
        else {
            // In this case, we swap+popped the node with no split, so we need to
            // do the same index again and iterate one less
            --stop;
        }
    }
}

// TODO: Shared
bool pointIsWithinAABB(const ui32v2& point, const ui32v4& aabb) {
    return point.x > aabb.x &&
           point.y > aabb.y &&
           point.x < aabb.x + aabb.z &&
           point.y < aabb.y + aabb.w;
}

// Returns false if we deleted the plot
bool CityPlotter::splitPlotByAABBIntersect(CityPlotIndex plotIndex, const ui32v4& aabb) {

    // Need to check fully enveloped cases
    // First test AABB+AABB collision to see if we even have a split
    if (!testAABBAABB_SIMD(mPlots[plotIndex].aabb, aabb)) {
        return true;
    }

    // TODO: Utility for getting corners
    const ui32v2 corners[4] = {
        { aabb.x, aabb.y },
        { aabb.x + aabb.z, aabb.y},
        { aabb.x, aabb.y + aabb.w },
        { aabb.x + aabb.z, aabb.y + aabb.w}
    };
    ui32v4 thisAABB = mPlots[plotIndex].aabb;
    bool intersects[4] = {
        pointIsWithinAABB(corners[0], thisAABB),
        pointIsWithinAABB(corners[1], thisAABB),
        pointIsWithinAABB(corners[2], thisAABB),
        pointIsWithinAABB(corners[3], thisAABB)
    };

    // TODO: Test edge case where the AABB is completely outside the box
    // Split with left to right bias
    // TODO: Randomize bias? Random direction? How can we generalize this method?
    // a = root
    if (intersects[enum_cast(CornerWinding::BOTTOM_LEFT)]) {
        // ____________________
        // |          |       |
        // |          |   d   |
        // |          *----*--|
        // |    a     |  c |e |
        // |          *----*--|
        // |          |   b   |
        // |__________|_______|
        // Do a vertical split
        CityPlotIndex plotB = splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_LEFT)], plotIndex, AXIS_VERTICAL);
        // Do a horizontal split on the new plot
        CityPlotIndex plotC = splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_RIGHT)], plotB, AXIS_HORIZONTAL);
        if (intersects[enum_cast(CornerWinding::TOP_LEFT)]) {
            // Split again along top right horizontal
            CityPlotIndex plotD = splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_LEFT)], plotC, AXIS_HORIZONTAL);
            if (intersects[enum_cast(CornerWinding::TOP_RIGHT)]) {
                // If we reach here, the entire quad is interior, split the interior plot and delete it
                mPlots[plotC] = mPlots[splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotC, AXIS_VERTICAL)];
                mPlots.pop_back();
            }
            else {
                // Need to delete and swap node as it is fully inside the aabb
                mPlots[plotC] = mPlots[plotD];
                mPlots.pop_back();
            }
        }
        else if (intersects[enum_cast(CornerWinding::BOTTOM_RIGHT)]) {
            // Have a free plot along the right, so split and delete interior
            mPlots[plotC] = mPlots[splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_RIGHT)], plotC, AXIS_VERTICAL)];
            mPlots.pop_back();
        }
        else {
            // New plot is fully enclosed in corner so delete it
            mPlots.pop_back();
        }
        return true;
    }
    else if (intersects[enum_cast(CornerWinding::BOTTOM_RIGHT)]) {
        // ____________________
        // |     |            |
        // |  d  |            |
        // |-----*   c        |
        // |  b  |            |
        // |-----*------------|
        // |         a        |
        // |__________________|
        CityPlotIndex plotB = splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_RIGHT)], plotIndex, AXIS_HORIZONTAL);
        CityPlotIndex plotC = splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_RIGHT)], plotB, AXIS_VERTICAL);
        if (intersects[enum_cast(CornerWinding::TOP_RIGHT)]) {
            // b is interior, split and swap and pop
            mPlots[plotB] = mPlots[splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotB, AXIS_HORIZONTAL)];
            mPlots.pop_back();
        }
        else {
            // Fully interior, so delete
            mPlots[plotB] = mPlots[plotC];
            mPlots.pop_back();
        }
        return true;
    } else if (intersects[enum_cast(CornerWinding::TOP_LEFT)]) {
        // ____________________
        // |          |       |
        // |          |       |
        // |          |  c    |
        // |    a     |       |
        // |          *---*---|
        // |          | b | d |
        // |__________|___|___|
        CityPlotIndex plotB = splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_VERTICAL);
        CityPlotIndex plotC = splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_LEFT)], plotB, AXIS_HORIZONTAL);
        if (intersects[enum_cast(CornerWinding::TOP_RIGHT)]) {
            // Split then swap+pop
            mPlots[plotB] = mPlots[splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotB, AXIS_VERTICAL)];
            mPlots.pop_back();
        }
        else {
            // We are fully interior, swap+pop
            mPlots[plotB] = mPlots[plotC];
            mPlots.pop_back();
        }
        return true;
    }
    else if (intersects[enum_cast(CornerWinding::TOP_RIGHT)]) {
        // ____________________
        // |                  |
        // |                  |
        // |        b         |
        // |                  |
        // |---*--------------|
        // | a |      c       |
        // |___|______________|
        CityPlotIndex plotB = splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_HORIZONTAL);
        // Swap and pop with split for last node
        mPlots[plotIndex] = mPlots[splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_VERTICAL)];
        mPlots.pop_back();
        return true;
    }
    else {
        // Else all the points lie completely outside the plot, which might mean the quad envelops us or splits us
        CityPlot& plot = mPlots[plotIndex];

        // TODO: Utility for getting corners
        const ui32v2 myCorners[4] = {
            { plot.aabb.x, plot.aabb.y },
            { plot.aabb.x + plot.aabb.z, plot.aabb.y},
            { plot.aabb.x, plot.aabb.y + plot.aabb.w },
            { plot.aabb.x + plot.aabb.z, plot.aabb.y + plot.aabb.w}
        };

        int numSplits = 0;

        // Test ray AABB intersects for the 4 sides of the AABB and split along those edges
        // Right ray
        if (corners[enum_cast(CornerWinding::TOP_RIGHT)].x < myCorners[enum_cast(CornerWinding::TOP_RIGHT)].x) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_VERTICAL);
            ++numSplits;
        }
        // Left ray
        if (corners[enum_cast(CornerWinding::TOP_LEFT)].x > myCorners[enum_cast(CornerWinding::TOP_LEFT)].x) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_VERTICAL);
            ++numSplits;
            // Delete our new plot because its inside
            mPlots.pop_back();
        }
        else if (numSplits) {
            // We are fully inside the box on the left of the first split, so pop and swap
            mPlots[plotIndex] = mPlots.back();
            mPlots.pop_back();
        }

        const int prevSplits = numSplits;
        // Top Ray
        if (corners[enum_cast(CornerWinding::TOP_LEFT)].y < myCorners[enum_cast(CornerWinding::TOP_LEFT)].y) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(corners[enum_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_HORIZONTAL);
            ++numSplits;
        }
        // Bottom ray
        if (corners[enum_cast(CornerWinding::BOTTOM_LEFT)].y > myCorners[enum_cast(CornerWinding::BOTTOM_LEFT)].y) {
            // We know we intersect from aabb test, so we don't need to check if we go beyond
            splitPlotAlongAxis(corners[enum_cast(CornerWinding::BOTTOM_LEFT)], plotIndex, AXIS_HORIZONTAL);
            ++numSplits;
            // Delete our new plot because its inside
            mPlots.pop_back();
        }
        else if (numSplits > prevSplits) {
            // We are fully inside the box on the bottom of the first split, so pop and swap
            mPlots[plotIndex] = mPlots.back();
            mPlots.pop_back();
        }

        // Check if we are fully enveloped, and pop and swap if so
        if (!numSplits) {
            mPlots[plotIndex] = mPlots.back();
            mPlots.pop_back();
            // Failure case, we were enveloped with no split
            return false;
        }
        return true;
    }
}

CityPlotIndex CityPlotter::splitPlotAlongAxis(ui32v2 splitPoint, CityPlotIndex plot, int axis)
{
    // TODO: Optimize arithmetic if necessary
    int oppositeAxis = !axis;
    assert(axis == 0 || axis == 1);
    CityPlot& plotToSplit = mPlots[plot];
    ui32 offset = splitPoint[oppositeAxis] - plotToSplit.aabb[oppositeAxis];
    assert(offset != 0 && offset < plotToSplit.aabb[oppositeAxis] + plotToSplit.aabb[oppositeAxis + 2]);
    // TODO: pass down road neighbors
    // Add new plot
    ui32v4 newAABB = plotToSplit.aabb;
    newAABB[oppositeAxis] = plotToSplit.aabb[oppositeAxis] + offset;
    newAABB[oppositeAxis + 2] = plotToSplit.aabb[oppositeAxis + 2] - offset;
    // Shrink old plot
    plotToSplit.aabb[oppositeAxis + 2] = offset;
    
    // Finally emplace
    mPlots.emplace_back(newAABB, mPlots.size(), plotToSplit.parentDistrict);
    return mPlots.size() - 1;
}
