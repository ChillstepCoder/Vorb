#include "stdafx.h"

#include "CityPlotter.h"
#include "city/City.h"

#include "util/MathUtil.hpp"

#include "World.h"

#include "Random.h"

CityPlotter::CityPlotter(City& city) :
    mCity(city)
{
}

CityPlotter::~CityPlotter()
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
        addDistrict(DistrictType::Rural, nullptr, DISTRICT_SIZE);
    }
    else {
        ++mCurrentTier;
        // TODO: Data driven
        switch (mCurrentTier) {
            case 1: {
                CityDistrict* ruralParent = mDistricts.back().get();
                addDistrict(DistrictType::Farming, ruralParent, DISTRICT_SIZE);
                addDistrict(DistrictType::Residential, ruralParent, DISTRICT_SIZE);
                break;
            }
            case 2: {
                CityDistrict* residentialParent = mDistricts.back().get();
                addDistrict(DistrictType::Commercial, residentialParent, DISTRICT_SIZE);
                //addDistrict(DistrictTypes::Harbor, residentialParent);
                break;
            }
            case 3: {
                CityDistrict* commercialParent = mDistricts.back().get();
                addDistrict(DistrictType::Government, commercialParent, DISTRICT_SIZE);
                addDistrict(DistrictType::Military, mDistricts.back().get(), DISTRICT_SIZE); // Parent to government
                addDistrict(DistrictType::Industrial, commercialParent, DISTRICT_SIZE);
                break;
            }
            case 4: {
                CityDistrict* industrialParent = mDistricts.back().get();
                addDistrict(DistrictType::Residential, industrialParent, DISTRICT_SIZE);
                addDistrict(DistrictType::Commercial, mDistricts.back().get(), DISTRICT_SIZE); // Parent to Residential
                break;
            }
            case 5: {
                // TODO: More tiers
                assert(false);
            }
        }
    }
}

CityPlot* CityPlotter::tryReservePlotForBuilding(const ui32v2& minPlotDims, const ui32v2& maxPlotDims)
{
    // TODO: Not heap
    //std::vector<CityPlotIndex> oversizedPlots;
    for (auto&& plot : mPlots) {
        if (!plot->isFree) {
            continue;
        }

        // Min dims check
        if ((plot->aabb.width < minPlotDims.x || plot->aabb.height < minPlotDims.y) ||
            (plot->aabb.width < minPlotDims.y || plot->aabb.height < minPlotDims.x)) {
            continue;
        }

        // Max dims check
        if ((plot->aabb.width > maxPlotDims.x || plot->aabb.height > maxPlotDims.y) ||
            (plot->aabb.width > maxPlotDims.y || plot->aabb.height > maxPlotDims.x)) {
            // TODO: Check if a split would be good here
            // Since the plot is too big, maybe it can be split.
            // oversizedPlots.emplace_back(plot.id);
            continue;
        }

        // This plot is now reserved for any use, and can no longer change
        plot->isFree = false;
        return plot.get();
    }

    // TODO: Use oversizedPlots

    return nullptr;
}

i32v2 getDistrictXY(int districtIndex) {
    return i32v2(districtIndex % DISTRICT_GRID_WIDTH, districtIndex / DISTRICT_GRID_WIDTH);
}

CityDistrict* CityPlotter::addDistrict(DistrictType type, CityDistrict* parent, ui32 size)
{
    std::unique_ptr<CityDistrict> newDistrict = std::make_unique<CityDistrict>();
    newDistrict->type = type;

    // TODO: Rectangles?
    // Dimensions
    newDistrict->aabb.width = size;
    newDistrict->aabb.height = size;

    // Check if we have parent or we are root
    if (!parent) {
        // Initialize AABB
        const ui32v2& cityCenter = mCity.getCityCenterWorldPos();
        newDistrict->aabb.x = cityCenter.x - newDistrict->aabb.width / 2;
        newDistrict->aabb.y = cityCenter.y - newDistrict->aabb.height / 2;

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
            if (!parent->children[randDirection] && randDirection != e_cast(parent->parentDirection)) {
                // Possibly valid child, check if it fits in the grid

                newCoords = parentCoords + CARTESIAN_NORMALS[randDirection];
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
            case e_cast(Cartesian::DOWN):
                newDistrict->aabb.x = parent->aabb.x;
                newDistrict->aabb.y = parent->aabb.y - newDistrict->aabb.height;
                break;
            case e_cast(Cartesian::LEFT):
                newDistrict->aabb.x = parent->aabb.x - newDistrict->aabb.width;
                newDistrict->aabb.y = parent->aabb.y;
                break;
            case e_cast(Cartesian::RIGHT):
                newDistrict->aabb.x = parent->aabb.x + parent->aabb.width;
                newDistrict->aabb.y = parent->aabb.y;
                break;
            case e_cast(Cartesian::UP):
                newDistrict->aabb.x = parent->aabb.x;
                newDistrict->aabb.y = parent->aabb.y + parent->aabb.height;
                break;
        }

        ++parent->numChildren;
    }
    // Added the root plot
    const ui32 rootPlotIndex = (ui32)mPlots.size();
    mPlots.emplace_back(std::make_unique<CityPlot>(newDistrict->aabb, 0, newDistrict.get()));

    // Plot Roads
    constexpr ui32 MAIN_ROAD_WIDTH = 5;
    {
        const ui32v2 startPos(newDistrict->aabb.x, newDistrict->aabb.y + newDistrict->aabb.height / 2);
        addRoad(*newDistrict,
            startPos,
            ui32v2(startPos.x + newDistrict->aabb.width, startPos.y),
            MAIN_ROAD_WIDTH, AXIS_HORIZONTAL);
    }
    {
        const ui32v2 startPos(newDistrict->aabb.x + newDistrict->aabb.width / 2, newDistrict->aabb.y);
        addRoad(*newDistrict,
            startPos,
            ui32v2(startPos.x, startPos.y + newDistrict->aabb.height),
            MAIN_ROAD_WIDTH, AXIS_VERTICAL);
    }

    // For each new plot, split them into 16 smaller
    {
        // TODO: Splitting twice doesnt work cause roads
        constexpr int NUM_SPLITS = 1;
        for (int numSplits = 0; numSplits < NUM_SPLITS; ++numSplits) {
            const ui32 stop = (ui32)mPlots.size();
            for (ui32 i = rootPlotIndex; i < stop; ++i) {
                CityPlotIndex rightId = splitPlotAlongAxis(ui32v2(mPlots[i]->aabb.x + mPlots[i]->aabb.width / 2, 0), i, AXIS_VERTICAL, INVALID_ROAD_ID);
                const ui32v2 horizontalSplit = ui32v2(0, mPlots[i]->aabb.y + mPlots[i]->aabb.height / 2);
                splitPlotAlongAxis(horizontalSplit, i, AXIS_HORIZONTAL, INVALID_ROAD_ID);
                splitPlotAlongAxis(horizontalSplit, rightId, AXIS_HORIZONTAL, INVALID_ROAD_ID);
            }
        }
    }

    // Generate alleys for any unroaded plots
    generateAlleysForUnroadedPlots(*newDistrict);
    
    mDistricts.emplace_back(std::move(newDistrict));
    return mDistricts.back().get();
}

void CityPlotter::generateAlleysForUnroadedPlots(CityDistrict &district) {
    const ui32 stop = (ui32)mPlots.size();
    const ui32 roadStop = (ui32)mCity.mRoads.size();
    for (ui32 i = 0; i < stop; ++i) {
        CityPlot& plot = *mPlots[i];
        if (plot.getAdjacentRoadCount() == 0) {
            // Find a road to connect an alley to
            CityRoad* closestRoad = nullptr;
            int closestDistance = INT_MAX;
            ui32v2 newStart;
            ui32v2 newEnd;
            int axis;
            for (ui32 r = 0; r < roadStop; ++r) {
                CityRoad& road = *mCity.mRoads[r];
                if (road.startPos.y == road.endPos.y) {
                    // Vertical roads
                    if (plot.aabb.getTopLeft().y < road.startPos.y) {
                        // We are below the road
                        if (plot.aabb.pos.x > road.startPos.x && plot.aabb.pos.x < road.endPos.x) {
                            // Left side is in line with road
                            int distance = road.startPos.y - plot.aabb.getTopLeft().y;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newStart = plot.aabb.getBottomLeft();
                                newEnd = ui32v2(newStart.x, road.startPos.y);
                                axis = AXIS_VERTICAL;
                            }
                        }
                        else if (plot.aabb.getTopRight().x > road.startPos.x && plot.aabb.getTopRight().x < road.endPos.x) {
                            // Right side is in line with road
                            int distance = road.startPos.y - plot.aabb.getTopRight().y;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newStart = plot.aabb.getBottomRight();
                                newEnd = ui32v2(newStart.x, road.startPos.y);
                                axis = AXIS_VERTICAL;
                            }
                        }
                    }
                    else if (plot.aabb.getBottomLeft().y > road.startPos.y) {
                        // We are above the road
                        if (plot.aabb.pos.x > road.startPos.x && plot.aabb.pos.x < road.endPos.x) {
                            // Left side is in line with road
                            int distance = road.startPos.y - plot.aabb.getBottomLeft().y;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newEnd = plot.aabb.getTopLeft();
                                newStart = ui32v2(newEnd.x, road.startPos.y);
                                axis = AXIS_VERTICAL;
                            }
                        }
                        else if (plot.aabb.getBottomRight().x > road.startPos.x && plot.aabb.getBottomRight().x < road.endPos.x) {
                            // Right side is in line with road
                            int distance = road.startPos.y - plot.aabb.getBottomRight().y;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newEnd = plot.aabb.getTopRight();
                                newStart = ui32v2(newEnd.x, road.startPos.y);
                                axis = AXIS_VERTICAL;
                            }
                        }
                    }
                }
                else {
                    assert(road.startPos.x == road.endPos.x);
                    // Horizontal roads
                    if (plot.aabb.getTopRight().x < road.startPos.x) {
                        // We are left of the road
                        if (plot.aabb.pos.y > road.startPos.y && plot.aabb.pos.y < road.endPos.y) {
                            // Bottom side is in line with road
                            int distance = road.startPos.x - plot.aabb.getBottomRight().x;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newStart = plot.aabb.getBottomLeft();
                                newEnd = ui32v2(road.startPos.x, newStart.y);
                                axis = AXIS_HORIZONTAL;
                            }
                        }
                        else if (plot.aabb.getTopRight().y > road.startPos.y && plot.aabb.getTopRight().y < road.endPos.y) {
                            // Top side is in line with road
                            int distance = road.startPos.x - plot.aabb.getTopRight().x;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newStart = plot.aabb.getTopLeft();
                                newEnd = ui32v2(road.startPos.x, newStart.y);
                                axis = AXIS_HORIZONTAL;
                            }
                        }
                    }
                    else if (plot.aabb.getBottomLeft().x > road.startPos.x) {
                        // We are right of the road
                        if (plot.aabb.pos.y > road.startPos.y && plot.aabb.pos.y < road.endPos.y) {
                            // Bottom side is in line with road
                            int distance = road.startPos.x - plot.aabb.getBottomLeft().x;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newEnd = plot.aabb.getBottomRight();
                                newStart = ui32v2(road.startPos.x, newEnd.y);
                                axis = AXIS_HORIZONTAL;
                            }
                        }
                        else if (plot.aabb.getTopLeft().y > road.startPos.y && plot.aabb.getTopLeft().y < road.endPos.y) {
                            // Top side is in line with road
                            int distance = road.startPos.x - plot.aabb.getTopLeft().x;
                            if (distance < closestDistance) {
                                closestRoad = &road;
                                closestDistance = distance;
                                newEnd = plot.aabb.getTopRight();
                                newStart = ui32v2(road.startPos.x, newEnd.y);
                                axis = AXIS_HORIZONTAL;
                            }
                        }
                    }
                }
            }
            if (closestRoad) {
                addRoad(
                    district,
                    newStart,
                    newEnd,
                    1,
                    axis
                );
            }
        }
    }
}

bool CityPlotter::markDistrictTilesAsOwned(CityDistrict& district) {
    bool wasConflict = false;
    mCity.mWorld.efficientEnumTileAABB(district.aabb, [&wasConflict](Chunk& chunk, TileIndex tileIndex) {
        // TODO: Look into forcing branch prediction, we should rarely conflict
        const Tile& tile = chunk.getTileAt(tileIndex);
        if (tile.hasFlagMainThread(TileFlags::TILE_FLAG_IN_CITY)) {
            wasConflict = true;
        }
        else {
            chunk.setTileFlag(tileIndex, TileFlags::TILE_FLAG_IN_CITY);
        }
    });

    return wasConflict;
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
    road.axis = static_cast<AXIS_2D>(axis);
    if (axis == AXIS_VERTICAL) {
        assert(road.startPos.y < road.endPos.y);
        road.length = road.endPos.y - road.startPos.y;
        road.aabb.data = {
            road.startPos.x - road.width / 2,
            road.startPos.y,
            road.width,
            road.length
        };
    }
    else {
        assert(road.startPos.x < road.endPos.x);
        road.length = road.endPos.x - road.startPos.x;
        road.aabb.data = {
            road.startPos.x,
            road.startPos.y - road.width / 2,
            road.length,
            road.width
        };
    }
    district.roads.emplace_back(mCity.addRoad(road));

    // Don't do recursive splitting
    ui32 stop = (ui32)mPlots.size();
    for (ui32 i = 0; i < stop;) {
        if (splitPlotByAABBIntersect(i, road.aabb, &road)) {
            ++i;
        }
        else {
            // In this case, we swap+popped the node with no split, so we need to
            // do the same index again and iterate one less
            --stop;
        }
    }
}

// Returns false if we deleted the plot
bool CityPlotter::splitPlotByAABBIntersect(CityPlotIndex plotIndex, const ui32AABB2& aabb, OPT CityRoad* road) {

    const ui32AABB2& plotAABB = mPlots[plotIndex]->aabb;
    const ui32v2 plotBottomLeft = plotAABB.getBottomLeft();
    const ui32v2 plotBottomRight = plotAABB.getBottomRight();
    const ui32v2 plotTopLeft = plotAABB.getTopLeft();
    const ui32v2 plotTopRight = plotAABB.getTopRight();

    const RoadID roadId = road ? road->id : INVALID_ROAD_ID;

    tryConnectRoad(plotIndex, aabb, roadId);

    // Need to check fully enveloped cases
    // First test AABB+AABB collision to see if we even have a split
    if (!testAABBAABB_SIMD(mPlots[plotIndex]->aabb, aabb)) {
        return true;
    }

    ui32v2 aabbCorners[4];
    aabb.getCorners(aabbCorners);

    bool intersects[4] = {
        pointIsWithinAABB(aabbCorners[0], plotAABB),
        pointIsWithinAABB(aabbCorners[1], plotAABB),
        pointIsWithinAABB(aabbCorners[2], plotAABB),
        pointIsWithinAABB(aabbCorners[3], plotAABB)
    };


    // TODO: Test edge case where the AABB is completely outside the box
    // Split with left to right bias
    // TODO: Randomize bias? Random direction? How can we generalize this method?
    // a = root
    if (intersects[e_cast(CornerWinding::BOTTOM_LEFT)]) {
        // ____________________
        // |          |       |
        // |          |   d   |
        // |          *----*--|
        // |    a     |  c |e |
        // |          *----*--|
        // |          |   b   |
        // |__________|_______|

        // Do a vertical split
        CityPlotIndex plotB = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_LEFT)], plotIndex, AXIS_VERTICAL, roadId);
        // Do a horizontal split on the new plot
        CityPlotIndex plotC = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_RIGHT)], plotB, AXIS_HORIZONTAL, roadId);
        if (intersects[e_cast(CornerWinding::TOP_LEFT)]) {
            // Split again along top right horizontal
            CityPlotIndex plotD = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_LEFT)], plotC, AXIS_HORIZONTAL, roadId);
            if (intersects[e_cast(CornerWinding::TOP_RIGHT)]) {
                // If we reach here, the entire quad is interior, split the interior plot and delete it
                mPlots[plotC] = std::move(mPlots[splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotC, AXIS_VERTICAL, roadId)]);
                mPlots.pop_back();
            }
            else {
                // Need to delete and swap node as it is fully inside the aabb
                mPlots[plotC] = std::move(mPlots[plotD]);
                mPlots.pop_back();
            }
        }
        else if (intersects[e_cast(CornerWinding::BOTTOM_RIGHT)]) {
            // Have a free plot along the right, so split and delete interior
            mPlots[plotC] = std::move(mPlots[splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_RIGHT)], plotC, AXIS_VERTICAL, roadId)]);
            mPlots.pop_back();
        }
        else {
            // New plot is fully enclosed in corner so delete it
            mPlots.pop_back();
        }
        return true;
    }
    else if (intersects[e_cast(CornerWinding::BOTTOM_RIGHT)]) {
        // ____________________
        // |     |            |
        // |  d  |            |
        // |-----*   c        |
        // |  b  |            |
        // |-----*------------|
        // |         a        |
        // |__________________|

        CityPlotIndex plotB = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_RIGHT)], plotIndex, AXIS_HORIZONTAL, roadId);
        CityPlotIndex plotC = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_RIGHT)], plotB, AXIS_VERTICAL, roadId);
        if (intersects[e_cast(CornerWinding::TOP_RIGHT)]) {
            // b is interior, split and swap and pop
            mPlots[plotB] = std::move(mPlots[splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotB, AXIS_HORIZONTAL, roadId)]);
            mPlots.pop_back();
        }
        else {
            // Fully interior, so delete
            mPlots[plotB] = std::move(mPlots[plotC]);
            mPlots.pop_back();
        }
        return true;
    } else if (intersects[e_cast(CornerWinding::TOP_LEFT)]) {
        // ____________________
        // |          |       |
        // |          |       |
        // |          |  c    |
        // |    a     |       |
        // |          *---*---|
        // |          | b | d |
        // |__________|___|___|
        CityPlotIndex plotB = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_VERTICAL, roadId);
        CityPlotIndex plotC = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_LEFT)], plotB, AXIS_HORIZONTAL, roadId);
        if (intersects[e_cast(CornerWinding::TOP_RIGHT)]) {
            // Split then swap+pop
            mPlots[plotB] = std::move(mPlots[splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotB, AXIS_VERTICAL, roadId)]);
            mPlots.pop_back();
        }
        else {
            // We are fully interior, swap+pop
            mPlots[plotB] = std::move(mPlots[plotC]);
            mPlots.pop_back();
        }
        return true;
    }
    else if (intersects[e_cast(CornerWinding::TOP_RIGHT)]) {
        // ____________________
        // |                  |
        // |                  |
        // |        b         |
        // |                  |
        // |---*--------------|
        // | a |      c       |
        // |___|______________|
        CityPlotIndex plotB = splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_HORIZONTAL, roadId);
        // Swap and pop with split for last node
        mPlots[plotIndex] = std::move(mPlots[splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_VERTICAL, roadId)]);
        mPlots.pop_back();
        return true;
    }
    else {
        // Else all the points lie completely outside the plot, which might mean the quad envelops us or splits us
        CityPlot& plot = *mPlots[plotIndex];

        // TODO: Utility for getting corners
        const ui32v2 plotCorners[4] = {
            { plot.aabb.x, plot.aabb.y },
            { plot.aabb.x + plot.aabb.width, plot.aabb.y},
            { plot.aabb.x, plot.aabb.y + plot.aabb.height },
            { plot.aabb.x + plot.aabb.width, plot.aabb.y + plot.aabb.height}
        };

        int numSplits = 0;

        // Test ray AABB intersects for the 4 sides of the AABB and split along those edges
        // Right ray
        if (aabbCorners[e_cast(CornerWinding::TOP_RIGHT)].x < plotCorners[e_cast(CornerWinding::TOP_RIGHT)].x) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_RIGHT)], plotIndex, AXIS_VERTICAL, roadId);
            ++numSplits;
        }
        // Left ray
        if (aabbCorners[e_cast(CornerWinding::TOP_LEFT)].x > plotCorners[e_cast(CornerWinding::TOP_LEFT)].x) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_VERTICAL, roadId);
            ++numSplits;
            // Delete our new plot because its inside
            mPlots.pop_back();
        }
        else if (numSplits) {
            // We are fully inside the box on the left of the first split, so pop and swap
            mPlots[plotIndex] = std::move(mPlots.back());
            mPlots.pop_back();
        }

        const int prevSplits = numSplits;
        // Top Ray
        if (aabbCorners[e_cast(CornerWinding::TOP_LEFT)].y < plotCorners[e_cast(CornerWinding::TOP_LEFT)].y) {
            // We know we intersect from aabb test, so we dont need to check if we go beyond
            splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::TOP_LEFT)], plotIndex, AXIS_HORIZONTAL, roadId);
            ++numSplits;
        }
        // Bottom ray
        if (aabbCorners[e_cast(CornerWinding::BOTTOM_LEFT)].y > plotCorners[e_cast(CornerWinding::BOTTOM_LEFT)].y) {
            // We know we intersect from aabb test, so we don't need to check if we go beyond
            splitPlotAlongAxis(aabbCorners[e_cast(CornerWinding::BOTTOM_LEFT)], plotIndex, AXIS_HORIZONTAL, roadId);
            ++numSplits;
            // Delete our new plot because its inside
            mPlots.pop_back();
        }
        else if (numSplits > prevSplits) {
            // We are fully inside the box on the bottom of the first split, so pop and swap
            mPlots[plotIndex] = std::move(mPlots.back());
            mPlots.pop_back();
        }

        // Check if we are fully enveloped, and pop and swap if so
        if (!numSplits) {
            mPlots[plotIndex] = std::move(mPlots.back());
            mPlots.pop_back();
            // Failure case, we were enveloped with no split
            return false;
        }
        return true;
    }
}

CityPlotIndex CityPlotter::splitPlotAlongAxis(ui32v2 splitPoint, CityPlotIndex plot, int axis, RoadID roadID) {

    // TODO: Optimize arithmetic if necessary
    int oppositeAxis = !axis;
    assert(axis == 0 || axis == 1);
    CityPlot& plotToSplit = *mPlots[plot];
    ui32 offset = splitPoint[oppositeAxis] - plotToSplit.aabb[oppositeAxis];
    assert(offset != 0 && offset < plotToSplit.aabb[oppositeAxis] + plotToSplit.aabb[oppositeAxis + 2]);
    // Add new plot
    ui32AABB2 newAABB = plotToSplit.aabb;
    newAABB[oppositeAxis] = plotToSplit.aabb[oppositeAxis] + offset;
    newAABB[oppositeAxis + 2] = plotToSplit.aabb[oppositeAxis + 2] - offset;
    // Shrink old plot
    plotToSplit.aabb[oppositeAxis + 2] = offset;
    
    // Finally emplace
    CityPlot& newPlot = *mPlots.emplace_back(std::make_unique<CityPlot>(newAABB, (CityPlotIndex)mPlots.size(), plotToSplit.parentDistrict));

    // Adjust neighbor connections
    if (axis == AXIS_HORIZONTAL) {
        newPlot.setNeighborRoad(Cartesian::UP, plotToSplit.getNeighborRoad(Cartesian::UP));
        plotToSplit.setNeighborRoad(Cartesian::UP, roadID);
        newPlot.setNeighborRoad(Cartesian::DOWN, roadID);
        newPlot.setNeighborRoad(Cartesian::LEFT, plotToSplit.getNeighborRoad(Cartesian::LEFT));
        newPlot.setNeighborRoad(Cartesian::RIGHT, plotToSplit.getNeighborRoad(Cartesian::RIGHT));
    }
    else {
        newPlot.setNeighborRoad(Cartesian::RIGHT, plotToSplit.getNeighborRoad(Cartesian::RIGHT));
        plotToSplit.setNeighborRoad(Cartesian::RIGHT, roadID);
        newPlot.setNeighborRoad(Cartesian::LEFT, roadID);
        newPlot.setNeighborRoad(Cartesian::UP, plotToSplit.getNeighborRoad(Cartesian::UP));
        newPlot.setNeighborRoad(Cartesian::DOWN, plotToSplit.getNeighborRoad(Cartesian::DOWN));
    }
    return (CityPlotIndex)(mPlots.size() - 1);
}

void CityPlotter::tryConnectRoad(CityPlotIndex plotIndex, const ui32AABB2& roadAabb, RoadID roadID) {
    if (roadID == INVALID_ROAD_ID) {
        return;
    }
    CityPlot& plot = *mPlots[plotIndex];
    const ui32AABB2& plotAABB = plot.aabb;
    ui32v2 plotAABBCorners[4];
    plotAABB.getCorners(plotAABBCorners);

    ui32v2 roadAABBCorners[4];
    roadAabb.getCorners(roadAABBCorners);

    // Edge kiss hookup roads
    if (MathUtil::areParallelSegmentsTouching(
        roadAABBCorners[e_cast(CornerWinding::BOTTOM_LEFT)],
        roadAABBCorners[e_cast(CornerWinding::TOP_LEFT)],
        plotAABBCorners[e_cast(CornerWinding::BOTTOM_RIGHT)],
        plotAABBCorners[e_cast(CornerWinding::TOP_RIGHT)])) {
        plot.setNeighborRoad(Cartesian::RIGHT, roadID);
    }
    else if (MathUtil::areParallelSegmentsTouching(
        roadAABBCorners[e_cast(CornerWinding::BOTTOM_RIGHT)],
        roadAABBCorners[e_cast(CornerWinding::TOP_RIGHT)],
        plotAABBCorners[e_cast(CornerWinding::BOTTOM_LEFT)],
        plotAABBCorners[e_cast(CornerWinding::TOP_LEFT)])) {
        plot.setNeighborRoad(Cartesian::LEFT, roadID);
    }
    if (MathUtil::areParallelSegmentsTouching(
        roadAABBCorners[e_cast(CornerWinding::BOTTOM_LEFT)],
        roadAABBCorners[e_cast(CornerWinding::BOTTOM_RIGHT)],
        plotAABBCorners[e_cast(CornerWinding::TOP_LEFT)],
        plotAABBCorners[e_cast(CornerWinding::TOP_RIGHT)])) {
        plot.setNeighborRoad(Cartesian::UP, roadID);
    }
    else if (MathUtil::areParallelSegmentsTouching(
        roadAABBCorners[e_cast(CornerWinding::TOP_LEFT)],
        roadAABBCorners[e_cast(CornerWinding::TOP_RIGHT)],
        plotAABBCorners[e_cast(CornerWinding::BOTTOM_LEFT)],
        plotAABBCorners[e_cast(CornerWinding::BOTTOM_RIGHT)])) {
        plot.setNeighborRoad(Cartesian::DOWN, roadID);
    }
}
