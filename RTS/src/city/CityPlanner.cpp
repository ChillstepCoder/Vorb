#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"
#include "CityPlotter.h"
#include "BuildingDescriptionRepository.h"

#include "Random.h"

#include "World.h"
#include "ResourceManager.h"


// TODO: T1 City requires
// Lumberjacks, wooden buildings, fishing, multi-agent jobs (woodcutting, ect)
// farming, hunting, trading post, night watch, militia

// T2: Wholesale, shops, OPTIONAL: stonecutter

CityPlanner::CityPlanner(City& city)
    : mCity(city)
{
    mBuildingGenerator = std::make_unique<BuildingBlueprintGenerator>(mCity.mWorld.getResourceManager().getBuildingRepository(), mCity.getCityBuilder());
}

void CityPlanner::update() {

}


CityPlot* CityPlanner::tryPurchasePlot(const PlotRequestProps& props) {
    // TODO use all the other props stuff
    return mCity.getCityPlotter().tryReservePlotForBuilding(props.minBuildingDims, props.maxBuildingDim);
}

void CityPlanner::generatePlanForPlotAsyncThenSendToBuilder(CityPlot& plot, const nString& buildingDescriptionName) {
    assert(!plot.mPendingBlueprint);

    ui32v2 cityCenter = mCity.mCityCenterWorldPos;
    const BuildingDescriptionRepository& buildingRepo = mCity.mWorld.getResourceManager().getBuildingRepository();

    const float sizeAlpha = Random::xorshf96f();

    // Generate floorplan size
    // TODO: Dont just spam lumbermill
    const BuildingDescription& desc = buildingRepo.getBuildingDescription(buildingDescriptionName);
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
    plot.mPendingBlueprint = mBuildingGenerator->generateBlueprintAsyncThenSendToBuilder(desc, sizeAlpha, dir, plotDims, bottomLeftPos);
    plot.mPendingBlueprint->plotIndex = plot.plotIndex;
}

void CityPlanner::debugPrintBlueprint(std::unique_ptr<BuildingBlueprint>& bp) const {
    const BuildingDescriptionRepository& buildingRepo = mCity.mWorld.getResourceManager().getBuildingRepository();
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
