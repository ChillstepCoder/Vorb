#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"

#include "Random.h"

#define DEGUG_BLUEPRINT 1

CityPlanner::CityPlanner(City& city)
    : mCity(city)
{
    mBuildingGenerator = std::make_unique<BuildingBlueprintGenerator>(city.getBuildingRepository());
}

void CityPlanner::update()
{
    if (mCity.mBuildings.empty()) {
        generatePlan();
    }
}


std::unique_ptr<BuildingBlueprint> CityPlanner::recieveNextBlueprint()
{
    if (mBluePrints.empty()) {
        return nullptr;
    }
    std::unique_ptr<BuildingBlueprint> bp = std::move(mBluePrints.front());
    mBluePrints.pop_front();
    return bp;
}

void CityPlanner::generatePlan() {
    ui32v2 cityCenter = mCity.mCityCenterWorldPos;
    BuildingDescriptionRepository& buildingRepo = mCity.getBuildingRepository();

    const float sizeAlpha = Random::xorshf96f();

    // Generate floorplan size
    const BuildingDescription& desc = buildingRepo.getBuildingDescription("house");
    ui16v2 plotDims;
    plotDims.x = vmath::lerp(desc.widthRange.x, desc.widthRange.y, sizeAlpha);
    // TODO: Dynamic aspect ratio
    plotDims.y = plotDims.x * desc.minAspectRatio;

    ui32v2 bottomLeftPos = cityCenter; // TODO: Actual position
    std::unique_ptr<BuildingBlueprint> bp = mBuildingGenerator->generateBuilding(desc, sizeAlpha, Cartesian::DOWN, plotDims, bottomLeftPos);

    if (IS_ENABLED(DEGUG_BLUEPRINT)) {
        std::cout << "\nGenerated house:" << bp->nodes.size() << " " << sizeAlpha << "\n";
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

    mBluePrints.emplace_back(std::move(bp));

    return;
}
