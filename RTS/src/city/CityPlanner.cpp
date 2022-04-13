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
    mBuildingGenerator = std::make_unique<BuildingBlueprintGenerator>(Services::ResourceManager::ref().getBuildingRepository(), mCity.getCityBuilder());
}

void CityPlanner::update() {

}

CityPlot* CityPlanner::tryPurchasePlot(const PlotRequestProps& props, entt::entity newOwner) {
    // TODO use all the other props stuff
    CityPlot* plot = mCity.getCityPlotter().tryReservePlotForBuilding(props.minBuildingDims, props.maxBuildingDim);
    plot->mOwnerEntity = newOwner;
    return plot;
}

void CityPlanner::generatePlanForPlotAsyncThenSendToBuilder(CityPlot& plot, const nString& buildingDescriptionName, BuildingBlueprintFlags flags) {
    assert(!plot.mPendingBlueprint);

    ui32v2 cityCenter = mCity.mCityCenterWorldPos;
    const BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();

    const float sizeAlpha = Random::xorshf96f();

    // Generate floorplan size
    // TODO: Dont just spam lumbermill
    const BuildingDef& desc = buildingRepo.getBuildingDef(buildingDescriptionName);
    // TODO: rotation to road
    const ui16v2 plotDims(plot.aabb.dims);
    // TODO:  aspect ratio
    //plotDims.y = plotDims.x * desc.minAspectRatio;

    const ui32v2 bottomLeftPos(plot.aabb.pos); // TODO: Actual position
    Cartesian dir = Cartesian::UP;
    if (plot.neighborRoads[e_cast(Cartesian::LEFT)] != INVALID_ROAD_ID) {
        dir = Cartesian::LEFT;
    }
    else if (plot.neighborRoads[e_cast(Cartesian::RIGHT)] != INVALID_ROAD_ID) {
        dir = Cartesian::RIGHT;
    }
    else if (plot.neighborRoads[e_cast(Cartesian::UP)] != INVALID_ROAD_ID) {
        dir = Cartesian::DOWN;
    }
    plot.mPendingBlueprint = mBuildingGenerator->generateBlueprintAsyncThenSendToBuilder(desc, sizeAlpha, dir, plotDims, bottomLeftPos, plot.mOwnerEntity, flags);
    plot.mPendingBlueprint->plotIndex = plot.plotIndex;
}

void CityPlanner::debugPrintBlueprint(std::unique_ptr<BuildingBlueprint>& bp) const {
    const BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();
    std::cout << "\nGenerated house:" << bp->rooms.size() << " " << bp->aabb.dims.x << "\n";
    for (auto&& node : bp->rooms) {
        std::cout << "  node - " << *buildingRepo.getNameFromRoomDefID(node.roomDefId) << " " <<
            node.offsetFromZero.x << " " << node.offsetFromZero.y << "\n";
        for (int i = 0; i < node.numChildren; ++i) {
            const int childIndex = (int)node.childRooms[i];
            std::cout << "    child - " << childIndex << " type - " <<
                *buildingRepo.getNameFromRoomDefID(bp->rooms[childIndex].roomDefId) << "\n";
        }
    }
}
