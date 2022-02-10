#include "stdafx.h"

#include "BuildTask.h"


BuildTask::BuildTask(BuildingBlueprint& blueprint, std::vector<std::unique_ptr<ItemReservation>>&& sourceItems, std::vector<TileIndex>&& targetTiles) : mSourceItems(std::move(sourceItems)), mTargetTiles(std::move(targetTiles)) {

}

BuildTask::~BuildTask()
{

}

bool BuildTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    throw std::logic_error("The method or operation is not implemented.");
}
