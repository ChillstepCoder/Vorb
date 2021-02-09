#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"

#include "Random.h"

#define DEGUG_BLUEPRINT 1

CityPlanner::CityPlanner(City& city)
    : mCity(city)
{

}

void CityPlanner::update()
{
    if (mCity.mBuildings.empty()) {
        generatePlan();
    }
}

bool CityPlanner::recieveNextPlan(OUT PlannedBuilding& outPlan)
{
    if (mStandardPlans.empty()) {
        return false;
    }
    outPlan = mStandardPlans.front();
    mStandardPlans.pop_front();
    return true;
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
    // OLD
    mStandardPlans.emplace_back();
    PlannedBuilding& newPlan = mStandardPlans.back();
    Building& building = newPlan.mBuilding;

    building.mBottomLeftWorldPos = cityCenter;

    const int numWalls = 6;
    building.mWallSegmentOffsets.resize(6);
    building.mWallSegmentOffsets[0].x = 8;
    building.mWallSegmentOffsets[0].y = 4;
    building.mWallSegmentOffsets[1].x = 4;
    building.mWallSegmentOffsets[1].y = -10;
    building.mWallSegmentOffsets[2].x = -building.mWallSegmentOffsets[0].x - building.mWallSegmentOffsets[1].x;
    building.mWallSegmentOffsets[2].y = -building.mWallSegmentOffsets[0].y - building.mWallSegmentOffsets[1].y;

    // NEW
    const float sizeAlpha = Random::xorshf96f();
    std::unique_ptr<BuildingBlueprint> bp = mBuildingGenerator.generateBuilding(buildingRepo.getBuildingDescription("house"), sizeAlpha);
    // TODO: Real
    bp->rootWorldPos = cityCenter;

    if (IS_ENABLED(DEGUG_BLUEPRINT)) {
        std::cout << "\n\Generated house:" << bp->nodes.size() << " " << sizeAlpha << "\n";
        for (auto&& node : bp->nodes) {
            std::cout << "  node - " << *buildingRepo.getNameFromRoomTypeID(node.nodeType) << " " << 
                node.offsetFromRoot.x << " " << node.offsetFromRoot.y << "\n";
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
