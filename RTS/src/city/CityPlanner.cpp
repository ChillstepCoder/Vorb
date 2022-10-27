#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"
#include "CityPlotter.h"
#include "BuildingDescriptionRepository.h"

#include "math/Random.h"

#include "world/IWorld.h"
#include "resources/ResourceManager.h"


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
    Cartesian dir = Cartesian::NORTH;
    if (plot.neighborRoads[e_cast(Cartesian::WEST)] != INVALID_ROAD_ID) {
        dir = Cartesian::WEST;
    }
    else if (plot.neighborRoads[e_cast(Cartesian::EAST)] != INVALID_ROAD_ID) {
        dir = Cartesian::EAST;
    }
    else if (plot.neighborRoads[e_cast(Cartesian::NORTH)] != INVALID_ROAD_ID) {
        dir = Cartesian::SOUTH;
    }
    plot.mPendingBlueprint = mBuildingGenerator->generateBlueprintAsyncThenSendToBuilder(desc, sizeAlpha, dir, plotDims, bottomLeftPos, plot.mOwnerEntity, flags, 5.0f /*TODO: Pass in*/);
    plot.mPendingBlueprint->plotIndex = plot.plotIndex;
}

void CityPlanner::debugPrintBlueprint(std::unique_ptr<BuildingBlueprint>& bp) const {
    const BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();
    LOG_DEBUG("Generated house: dx {} dy {}", bp->rooms.size(), bp->aabb.dims.x);
    for (auto&& node : bp->rooms) {
        char nameBuf[64];
        buildingRepo.getNameFromRoomDefID(node.roomDefId).toString(nameBuf, nullptr);
        LOG_DEBUG("   node {} {} {}", nameBuf, node.offsetFromZero.x, node.offsetFromZero.y);
        for (int i = 0; i < node.numChildren; ++i) {
            const int childIndex = (int)node.childRooms[i];
            buildingRepo.getNameFromRoomDefID(bp->rooms[childIndex].roomDefId).toString(nameBuf, nullptr);
            LOG_DEBUG("    child - {} type - {}", childIndex, nameBuf);
        }
    }
}
