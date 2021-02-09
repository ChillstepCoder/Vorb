#include "stdafx.h"
#include "City.h"

#include "CityPlanner.h"
#include "CityBuilder.h"
#include "World.h"
#include "ResourceManager.h"

City::City(const ui32v2& cityCenterWorldPos, World& world)
    : mCityCenterWorldPos(cityCenterWorldPos)
    , mWorld(world)
    , mBuildingRepository(mWorld.getResourceManager().getBuildingRepository())
{

    TileHandle root = mWorld.getTileHandleAtWorldPos(f32v2(cityCenterWorldPos));
    mChunks.push_back(root.getMutableChunk());
    // This belongs to us, don't go away
    mChunks.back()->incRef();

    mCityPlanner = std::make_unique<CityPlanner>(*this);
    mCityBuilder = std::make_unique<CityBuilder>(*this, mWorld);
}

City::~City() {

}

void City::update(float deltaTime)
{
    UNUSED(deltaTime);
    // TODO: tick()
    mCityPlanner->update();
    mCityBuilder->update();
}

void City::tick() {
    assert(false);
}
