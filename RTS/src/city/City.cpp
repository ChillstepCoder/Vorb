#include "stdafx.h"
#include "City.h"

#include "CityPlotter.h"
#include "CityPlanner.h"
#include "CityBuilder.h"
#include "CityResidentManager.h"
#include "CityBusinessManager.h"
#include "CityQuartermaster.h"
#include "BuildingBlueprintGenerator.h"
#include "BuildingRepository.h"
#include "ecs/business/BusinessRepository.h"
#include "world/World.h"
#include "world/IChunkGrid.h"
#include "resources/ResourceManager.h"

#include "ecs/IEntityComponentSystem.h"

City::City(World& world, const ui32v2& cityCenterWorldPos)
    : mWorld(world)
    , mCityCenterWorldPos(cityCenterWorldPos)
    , mCityAABB(mCityCenterWorldPos.x, mCityCenterWorldPos.y, 6, 6)
{

    TileHandle root = mWorld.getTerrainTileHandleAtWorldPos(f32v2(cityCenterWorldPos));
    mChunks.push_back(&mWorld.getChunkGrid().getChunk(root.getChunkIDAtPos()));
    // This belongs to us, don't go away
    // TODO: Need to release later
    mChunks.back()->incRef();

    mCityBuilder = std::make_unique<CityBuilder>(*this);
    mCityPlotter = std::make_unique<CityPlotter>(*this);
    mCityPlanner = std::make_unique<CityPlanner>(*this);
    mCityResidentManager = std::make_unique<CityResidentManager>(*this);
    mCityBusinessManager = std::make_unique<CityBusinessManager>(*this);
    mCityQuartermaster = std::make_unique<CityQuartermaster>(*this);

    mCityPlotter->initAsTier(0);
    //mCityQuartermaster->tryCreateCityStockpileAt(mCityAABB);

    // Add test business
    Services::ResourceManager::ref().getBusinessRepository().createBusinessEntity(this, mWorld.getECS().mRegistry, "lumbermill");
}

City::~City() {

}

void City::update()
{
    mCityPlanner->update();
    mCityBuilder->update();
}

void City::addResidentToCity(entt::entity entity) {

    mCityResidentManager->addResident(entity);
}

void City::removeResidentFromCity(entt::entity entity) {

    mCityResidentManager->removeResident(entity);
}

void City::tick() {
    assert(false);
}

// Given three colinear points p, q, r, the function checks if 
// point q lies on line segment 'pr' 
bool onSegment(ui32v2 p, ui32v2 q, ui32v2 r) {

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
int orientation(f32v2 p, f32v2 q, f32v2 r)
{
    // See https://www.geeksforgeeks.org/orientation-3-ordered-points/ 
    // for details of below formula. 
    const f32 val = (q.y - p.y) * (r.x - q.x) -
              (q.x - p.x) * (r.y - q.y);

    if (abs(val) < 0.0001f) return 0;  // colinear 

    return (val > 0) ? 1 : 2; // clock or counterclock wise 
}

// The main function that returns true if line segment 'p1q1' 
// and 'p2q2' intersect. 
// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
bool doIntersect(f32v2 p1, f32v2 q1, f32v2 p2, f32v2 q2)
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
    return doIntersect(roadA.startPos, roadA.endPos, roadB.startPos, roadB.endPos);
}

RoadID City::addRoad(CityRoad& road)
{
    // It is OUR job to set up neighbors
    assert(road.neighborRoads.empty());
    RoadID id = (RoadID)mRoads.size();
    road.id = id;
    mRoads.emplace_back(std::make_unique<CityRoad>(road));
    CityRoad& newRoad = *mRoads.back();
    // Set up neighbors for pathing and such
    for (size_t i = 0; i < mRoads.size() - 1; ++i) {
        // Intersection test
        CityRoad& road = *mRoads[i];
        if (roadDoesIntersect(newRoad, road)) {
            // Compute the distances along each road
            ui32 d1, d2;
            if (newRoad.axis == AXIS_HORIZONTAL) {
                if (road.axis == AXIS_VERTICAL) {
                    d1 = road.startPos.x - newRoad.startPos.x;
                    d2 = newRoad.startPos.y - road.startPos.y;
                }
                else {
                    // Colinear
                    if (road.endPos.x == newRoad.startPos.x) {
                        d1 = 0;
                        d2 = road.length;
                    }
                    else {
                        d1 = newRoad.length;
                        d2 = 0;
                    }
                }
            }
            else {
                if (road.axis == AXIS_HORIZONTAL) {
                    d1 = road.startPos.y - newRoad.startPos.y;
                    d2 = newRoad.startPos.x - road.startPos.x;
                }
                else {
                    // Colinear
                    if (road.endPos.y == newRoad.startPos.y) {
                        d1 = 0;
                        d2 = road.length;
                    }
                    else {
                        d1 = newRoad.length;
                        d2 = 0;
                    }
                }
            }
            // TODO: Sort based on distance
            newRoad.neighborRoads.emplace_back(std::make_pair(d1, &road));
            road.neighborRoads.emplace_back(std::make_pair(d2, &newRoad));
        }
    }

    // Update tile flags with road info
    /*i32v2 worldPos;
    for (worldPos.y = newRoad.aabb.pos.y; worldPos.y < newRoad.aabb.pos.y + newRoad.aabb.dims.y; ++worldPos.y) {
        for (worldPos.x = newRoad.aabb.pos.x; worldPos.x < newRoad.aabb.pos.x + newRoad.aabb.dims.x; ++worldPos.x) {
            TileHandle handle = mWorld.getTileHandleAtWorldPos(worldPos);
            handle.getMutableChunk()->setTileFlag(handle.index, TileFlags::TILE_FLAG_ROAD);
        }
    }*/

    mCityBuilder->addRoadToBuild(id);
    return id;
}


void CityGraph::update() {
    for (auto&& it : mNodes) {
        it->update();
    }
}

City* CityGraph::getClosestCityToPoint(const f32v2& pos) const
{
    City* closest = nullptr;
    f32 closestDist2 = FLT_MAX;
    for (auto&& city : mNodes) {
        const f32 dist2 = glm::length2(f32v2(city->getCityCenterWorldPos()) - pos);
        if (dist2 < closestDist2) {
            closestDist2 = dist2;
            closest = city.get();
        }
    }
    return closest;
}

void CityGraph::createCityAt(const i32v2& worldPos) {
    std::unique_ptr<City> newCity = std::make_unique<City>(mWorld, worldPos);
    mNodes.emplace_back(std::move(newCity));
}
