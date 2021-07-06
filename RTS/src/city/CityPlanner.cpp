#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"
#include "CityPlotter.h"

#include "Random.h"

#define DEGUG_BLUEPRINT 1

// TODO: T1 City requires
// Lumberjacks, wooden buildings, fishing, multi-agent jobs (woodcutting, ect)
// farming, hunting, trading post, night watch, militia

// T2: Wholesale, shops, OPTIONAL: stonecutter

CityPlanner::CityPlanner(City& city)
    : mCity(city)
{
    mBuildingGenerator = std::make_unique<BuildingBlueprintGenerator>(city.getBuildingRepository());
}

void CityPlanner::update()
{

    // Update blueprints that are finished generating
    for (size_t i = 0; i < mGeneratingBlueprints.size();) {
        if (!mGeneratingBlueprints[i]->isGenerating) {
            // Swap to end so we can remove from list
            mGeneratingBlueprints[i].swap(mGeneratingBlueprints.back());
            // Emplace null unique ptr
            mFinishedBluePrints.emplace_back();
            // Move from the generating to the finished list
            mGeneratingBlueprints.back().swap(mFinishedBluePrints.back());
            // Release the null unique_ptr
            mGeneratingBlueprints.pop_back();
        }
        else {
            ++i;
        }
    }

    if (!mHasFreePlots) {
        return;
    }

    // TODO: THIS IS TEMPORARILY DISABLED
    return;

    CityPlot* plotForBuilding = mCity.getCityPlotter().reservePlotForBuilding(ui32v2(1, 1), ui32v2(100, 100));
    if (plotForBuilding) {
        generatePlan(*plotForBuilding);
    }
    else {
        // Don't try anymore
        mHasFreePlots = false;
    }
}


std::unique_ptr<BuildingBlueprint> CityPlanner::recieveNextBlueprint()
{
    if (mFinishedBluePrints.empty()) {
        return nullptr;
    }
    std::unique_ptr<BuildingBlueprint> bp = std::move(mFinishedBluePrints.front());
    mFinishedBluePrints.pop_front();
    return bp;
}

void CityPlanner::generatePlan(CityPlot& plot) {
    ui32v2 cityCenter = mCity.mCityCenterWorldPos;
    BuildingDescriptionRepository& buildingRepo = mCity.getBuildingRepository();

    const float sizeAlpha = Random::xorshf96f();

    // Generate floorplan size
    // TODO: Dont just spam lumbermill
    const BuildingDescription& desc = buildingRepo.getBuildingDescription("lumbermill");
    // TODO: rotation to road
    const ui16v2 plotDims(plot.aabb.dims);
    // TODO:  aspect ratio
    //plotDims.y = plotDims.x * desc.minAspectRatio;

    const ui32v2 bottomLeftPos(plot.aabb.pos); // TODO: Actual position
    Cartesian dir = Cartesian::UP;
    if (plot.neighborRoads[enum_cast(Cartesian::LEFT)] != INVALID_ROAD_ID) {
        dir = Cartesian::LEFT;
    }
    else if (plot.neighborRoads[enum_cast(Cartesian::RIGHT)] != INVALID_ROAD_ID) {
        dir = Cartesian::RIGHT;
    }
    else if (plot.neighborRoads[enum_cast(Cartesian::UP)] != INVALID_ROAD_ID) {
        dir = Cartesian::DOWN;
    }
    auto bp = mBuildingGenerator->generateBuildingAsync(desc, sizeAlpha, dir, plotDims, bottomLeftPos);
    bp->plotIndex = plot.plotIndex;
    mGeneratingBlueprints.emplace_back(std::move(bp));
    return;
}

void CityPlanner::finishBlueprint(std::unique_ptr<BuildingBlueprint>&& bp)
{
    if (IS_ENABLED(DEGUG_BLUEPRINT)) {
        BuildingDescriptionRepository& buildingRepo = mCity.getBuildingRepository();
        std::cout << "\nGenerated house:" << bp->nodes.size() << " " << bp->dims.x << "\n";
        for (auto&& node : bp->nodes) {
            std::cout << "  node - " << *buildingRepo.getNameFromRoomTypeID(node.nodeType) << " " <<
                node.offsetFromZero.x << " " << node.offsetFromZero.y << "\n";
            for (int i = 0; i < node.numChildren; ++i) {
                const int childIndex = (int)node.childRooms[i];
                std::cout << "    child - " << childIndex << " type - " <<
                    *buildingRepo.getNameFromRoomTypeID(bp->nodes[childIndex].nodeType) << "\n";
            }
        }
    }

    mFinishedBluePrints.emplace_back(std::move(bp));

}
