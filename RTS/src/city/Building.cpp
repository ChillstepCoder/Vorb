#include "stdafx.h"
#include "Building.h"

#include "world/settlement/building/BuildingBlueprint.h"

Building::Building() = default;
Building::~Building() = default;
Building::Building(Building&& other) noexcept = default;
Building& Building::operator=(Building&& other) noexcept = default;

void Building::setBlueprint(std::unique_ptr<BuildingBlueprint>&& bp) {
    mBlueprint = std::move(bp);
}