#include "stdafx.h"

#include "CityPlanner.h"
#include "City.h"

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
    mStandardPlans.pop();
    return true;
}

void CityPlanner::generatePlan() {
    ui32v2 cityCenter = mCity.mCityCenterWorldPos;

    mStandardPlans.emplace();
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

}
