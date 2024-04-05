#include "stdafx.h"
#include "building/building.h"

#include "world/World.h"
#include "tile/TileContainerRepository.h"

#include "building/BuildingBlueprint.h"

Building::Building() = default;
Building::~Building() = default;
Building::Building(Building&& other) noexcept = default;
Building& Building::operator=(Building&& other) noexcept = default;

void Building::setBlueprint(std::unique_ptr<BuildingBlueprint>&& bp) {
    mBlueprint = std::move(bp);
}

void Building::freeData() {
    ASSERT_GAME_THREAD();
    if (mTileContainer) {
        mTileContainer->getWorld().getTileContainerRepository().destroyTileContainer(mTileContainer);
        mTileContainer = nullptr;
    }
}
