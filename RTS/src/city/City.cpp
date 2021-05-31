#include "stdafx.h"
#include "City.h"

#include "CityPlotter.h"
#include "CityPlanner.h"
#include "CityBuilder.h"
#include "CityResidentManager.h"
#include "CityFunctionManager.h"
#include "CityBusinessManager.h"
#include "ecs/business/BusinessRepository.h"
#include "World.h"
#include "ResourceManager.h"

#include "ecs/EntityComponentSystem.h"

City::City(const ui32v2& cityCenterWorldPos, World& world)
    : mCityCenterWorldPos(cityCenterWorldPos)
    , mWorld(world)
    , mBuildingRepository(mWorld.getResourceManager().getBuildingRepository())
    , mCityAABB(mCityCenterWorldPos.x, mCityCenterWorldPos.y, 1, 1)
{

    TileHandle root = mWorld.getTileHandleAtWorldPos(f32v2(cityCenterWorldPos));
    mChunks.push_back(root.getMutableChunk());
    // This belongs to us, don't go away
    mChunks.back()->incRef();

    mCityPlotter = std::make_unique<CityPlotter>(*this);
    mCityPlanner = std::make_unique<CityPlanner>(*this);
    mCityBuilder = std::make_unique<CityBuilder>(*this, mWorld);
    mCityResidentManager = std::make_unique<CityResidentManager>(*this);
    mCityFunctionManager = std::make_unique<CityFunctionManager>(*this);
    mCityBusinessManager = std::make_unique<CityBusinessManager>(*this);

    mCityPlotter->initAsTier(0);

    // Add test business
    mWorld.getResourceManager().getBusinessRepository().createBusinessEntity(this, mWorld.getECS().mRegistry, "lumbermill");
}

City::~City() {

}

void City::update(float deltaTime)
{
    UNUSED(deltaTime);
    // TODO: tick()
    mCityPlanner->update();
    mCityBuilder->update();
    mCityFunctionManager->update();
}

void City::addResidentToCity(entt::entity entity)
{
    mCityResidentManager->addResident(entity);
}

void City::removeResidentFromCity(entt::entity entity)
{
    mCityResidentManager->removeResident(entity);
}

void City::tick() {
    assert(false);
}

// Given three colinear points p, q, r, the function checks if 
// point q lies on line segment 'pr' 
bool onSegment(ui32v2 p, ui32v2 q, ui32v2 r)
{
    if (q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
        q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y))
        return true;

    return false;
}

// To find orientation of ordered triplet (p, q, r). 
// The function returns following values 
// 0 --> p, q and r are colinear 
// 1 --> Clockwise 
// 2 --> Counterclockwise 
int orientation(ui32v2 p, ui32v2 q, ui32v2 r)
{
    // See https://www.geeksforgeeks.org/orientation-3-ordered-points/ 
    // for details of below formula. 
    int val = (q.y - p.y) * (r.x - q.x) -
        (q.x - p.x) * (r.y - q.y);

    if (val == 0) return 0;  // colinear 

    return (val > 0) ? 1 : 2; // clock or counterclock wise 
}

// The main function that returns true if line segment 'p1q1' 
// and 'p2q2' intersect. 
bool doIntersect(ui32v2 p1, ui32v2 q1, ui32v2 p2, ui32v2 q2)
{
    // Find the four orientations needed for general and 
    // special cases 
    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    // General case 
    if (o1 != o2 && o3 != o4)
        return true;

    // Special Cases 
    // p1, q1 and p2 are colinear and p2 lies on segment p1q1 
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;

    // p1, q1 and q2 are colinear and q2 lies on segment p1q1 
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;

    // p2, q2 and p1 are colinear and p1 lies on segment p2q2 
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;

    // p2, q2 and q1 are colinear and q1 lies on segment p2q2 
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;

    return false; // Doesn't fall in any of the above cases 
}
// TODO: Optimize? Find intersection distance?
bool roadDoesIntersect(const CityRoad& roadA, const CityRoad& roadB) {
    return doIntersect(roadA.startPos, roadB.startPos, roadA.endPos, roadB.endPos);
}

RoadID City::addRoad(CityRoad& road)
{
    // It is OUR job to set up neighbors
    assert(road.neighborRoads.empty());
    RoadID id = mRoads.size();
    road.id = id;
    mRoads.emplace_back(road);
    CityRoad& newRoad = mRoads.back();
    // Set up neighbors for pathing and such
    for (size_t i = 0; i < mRoads.size() - 1; ++i) {
        // Intersection test
        CityRoad& road = mRoads[i];
        if (roadDoesIntersect(newRoad, road)) {
            newRoad.neighborRoads.emplace_back(i);
            road.neighborRoads.emplace_back(mRoads.size() - 1);
        }
    }
    mCityBuilder->addRoadToBuild(id);
    return id;
}

BuildingID City::addCompletedBuilding(Building&& building) {
    mBuildings.emplace_back(building);
    Building& newBuilding = mBuildings.back();
    newBuilding.mId = mBuildings.size() - 1;
    
    mCityFunctionManager->registerNewBuilding(newBuilding);

    return newBuilding.mId;
}
