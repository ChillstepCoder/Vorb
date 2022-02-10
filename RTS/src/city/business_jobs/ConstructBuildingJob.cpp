#include "stdafx.h"
#include "ConstructBuildingJob.h"

#include "city/BuildingBlueprint.h"
#include "ecs/business/BusinessComponent.h"

#include "item/ItemStockpile.h"

#include "ai/tasks/BuildTask.h"


JobRequiredItems::JobRequiredItems()
{

}

JobRequiredItems::~JobRequiredItems()
{

}


ConstructBuildingJob::ConstructBuildingJob(BuildingBlueprint& blueprint) : mBlueprint(blueprint) {
    assert(!mBlueprint.isGenerating);
    const std::vector<BlueprintTile>& tiles = mBlueprint.tiles;
    // Track required items internally
    mRequiredItems.resize(mBlueprint.requiredItemsToBuild.size());
    for (size_t i = 0; i < mRequiredItems.size(); ++i) {
        JobRequiredItems& required = mRequiredItems[i];
        ItemStack& stack = mBlueprint.requiredItemsToBuild[i];
        required.id = stack.id;
        required.quantityRequired = stack.quantity;
    }
    mFirstUnfinishedTileIndex = UINT32_MAX;

    mTilesToConstruct.resize(tiles.size(), TilesToConstruct{ ConstructTileState::DONE, false });
    for (size_t i = 0; i < mTilesToConstruct.size(); ++i) {
        switch (tiles[i].type) {
            case BlueprintTileType::NONE:
                break;
            case BlueprintTileType::WALL:
                mTilesToConstruct[i].state = ConstructTileState::WAITING_CONSTRUCT_GROUND;
                ++mNumGroundTilesToConstruct;
                if (mFirstUnfinishedTileIndex == UINT32_MAX) mFirstUnfinishedTileIndex = i;
                break;
            case BlueprintTileType::FLOOR_1:
                mTilesToConstruct[i].state = ConstructTileState::WAITING_CONSTRUCT_GROUND;
                ++mNumGroundTilesToConstruct;
                if (mFirstUnfinishedTileIndex == UINT32_MAX) mFirstUnfinishedTileIndex = i;
                break;
            case BlueprintTileType::DOOR:
                mTilesToConstruct[i].state = ConstructTileState::WAITING_CONSTRUCT_TOP;
                ++mNumTopTilesToConstruct;
                if (mFirstUnfinishedTileIndex == UINT32_MAX) mFirstUnfinishedTileIndex = i;
                break;
            case BlueprintTileType::TYPES:
            default:
                assert(false);
                break;
        }
    }

    assert(mFirstUnfinishedTileIndex != UINT32_MAX);

    // Just for error checking
    ui32 totalTilesToBuild = mNumGroundTilesToConstruct + mNumMidTilesToConstruct + mNumTopTilesToConstruct;
    assert(totalTilesToBuild == mBlueprint.totalTilesToBuild);
}
static_assert(enum_cast(BlueprintTileType::TYPES) == 4, "Update build logic");

ConstructBuildingJob::~ConstructBuildingJob() {

}

bool ConstructBuildingJob::tick(World& world, entt::registry& registry, entt::entity business) {

    if (isDone()) {
        return true;
    }

    // Without idle workers we cant do anything
    BusinessComponent& businessCmp = registry.get<BusinessComponent>(business);

    // Search for items if we need them
    for (auto&& item : mRequiredItems) {
        if (item.quantityReserved < item.quantityRequired) {
            tryReserveItems(item, businessCmp);
        }
    }

    // Return true when we are done
    return false;
}

float ConstructBuildingJob::getProgress() const {
    ui32 totalTilesConstructedThusFar = mBlueprint.totalTilesToBuild - (mNumGroundTilesToConstruct + mNumMidTilesToConstruct + mNumTopTilesToConstruct);
    if (totalTilesConstructedThusFar == 0) return 0.0f;
    return (f32)mBlueprint.totalTilesToBuild / (f32)totalTilesConstructedThusFar;
}

IAgentTaskPtr ConstructBuildingJob::tryMakeTaskForWorker(entt::entity worker) {
    if (mTotalResourcesReserved && mNumTilesReservedInTasks < (mBlueprint.totalTilesToBuild - mBlueprint.tilesBuilt)) {
        constexpr ui32 TILES_TO_BUILD_PER_JOB = 5;
        const ui32 tilesForJob = glm::min((mBlueprint.totalTilesToBuild - mBlueprint.tilesBuilt) - mNumTilesReservedInTasks, TILES_TO_BUILD_PER_JOB);
        mNumTilesReservedInTasks += tilesForJob;

        std::vector<std::unique_ptr<ItemReservation>> sourceItems;
        std::vector<TileIndex> targetTiles;
        return std::make_shared<BuildTask>(mBlueprint, std::move(sourceItems), std::move(targetTiles));
    }
    return nullptr;
}

void ConstructBuildingJob::tryReserveItems(JobRequiredItems& item, BusinessComponent& businessCmp) {
    ItemStack itemsRequired;
    itemsRequired.id = item.id;
    itemsRequired.quantity = item.quantityRequired - item.quantityReserved;
    for (auto&& stockpile : businessCmp.mOwnedStockpiles) {
        std::unique_ptr<ItemReservation> reservation = stockpile->tryReserveItemStack(itemsRequired, 1);
        if (reservation) {
            mTotalResourcesReserved += reservation->getRemainingQuantity();
            item.quantityReserved += reservation->getRemainingQuantity();
            itemsRequired.quantity -= reservation->getRemainingQuantity();
            item.mReservations.push_back(std::move(reservation));

            if (itemsRequired.quantity == 0) {
                break;
            }
        }
    }
}
