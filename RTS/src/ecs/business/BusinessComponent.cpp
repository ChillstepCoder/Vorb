#include "stdafx.h"
#include "BusinessComponent.h"

#include "city/City.h"
#include "city/CityBuilder.h"
#include "city/BuildingBlueprint.h"

#include "ecs/component/OwnershipComponent.h"

#include "world/IWorld.h"
#include "resources/ResourceManager.h"
#include "city/BuildingDescriptionRepository.h"
#include "city/business_jobs/ConstructBuildingJob.h"
#include "tile/TileScanner.h"
#include "item/ItemStockpile.h"
#include "resources/TileRepository.h"

#include "debugging/DebugRenderer.h"
#include "options/DebugOptions.h"

#include "city/CityPlanner.h"

#include "definitions/BusinessDef.h"

#include "ai/tasks/GatherTask.h"
#include "ecs/component/EmployeeComponent.h"

const int UPDATE_INTERVAL = 60;

// TODO: Smarter scanning, dont scan same area twice
constexpr int SCAN_FRAMES_DELAY = 600; 
constexpr int MAX_TILES_TO_SCAN_FOR = 64;
constexpr ui32 MAX_SCAN_DISTANCE = 128;
constexpr ui32 MAX_RETURN_TILES = 32;
constexpr ui32 IDLE_CAPACITY_INC = 5;

constexpr ui32 TASK_LIST_MAX_SIZE = 50;


BusinessComponent::BusinessComponent() {
    mIdleWorkers.set_capacity(IDLE_CAPACITY_INC);
}

//IAgentTaskPtr BusinessComponent::aquireTask() {
//    if (mTasksToDo.empty()) return nullptr;
//
//    for (auto&& it = mTasksToDo.begin(); it != mTasksToDo.end(); ++it) {
//        if (it->second.size()) {
//            IAgentTaskPtr task = it->second.front();
//            it->second.pop_front();
//            if (it->second.empty()) {
//                mTasksToDo.erase(it);
//            }
//            return std::move(task);
//        }
//    }
//
//    return nullptr;
//}

void BusinessComponent::addIdleWorker(entt::entity worker) {
    if (mIdleWorkers.size() == mIdleWorkers.capacity()) {
        mIdleWorkers.set_capacity(mIdleWorkers.capacity() + IDLE_CAPACITY_INC);
    }
    std::cout << "ADD IDLE " << mIdleWorkers.size() << " " << mIdleWorkers.capacity() << std::endl;
    mIdleWorkers.push_back(worker);
}

BusinessSystem::BusinessSystem()
{
}

// TODO: Rename to task type?
enum TaskPriorities {
    TASK_PRIORITY_RETAIL,
    TASK_PRIORITY_BUILD,
    TASK_PRIORITY_GATHER,
};

void updateGatherComponent(entt::registry& registry, BusinessGatherComponent& gatherCmp, BusinessComponent& businessCmp, OwnershipComponent& ownershipCmp) {
    // Gathering currently requires a city
    assert(businessCmp.mCity);
    
    // Scans
    // TODO: Better support for multiple plots
    if (gatherCmp.mScannedTiles.empty()) {
        for (auto&& ownedPlot : ownershipCmp.mOwnedPlots) {
            PreciseTimer timer;
            gatherCmp.mScannedTiles = TileScanner::scanForResource(gatherCmp.mResourceToGather, ownedPlot->aabb.getCenter(), MAX_SCAN_DISTANCE, MAX_RETURN_TILES);
            std::cout << " Tile scanning took " << timer.stop() << " ms and returned " << gatherCmp.mScannedTiles.size() << " tiles\n";
            if (sDebugOptions.mShowPaths) {
                for (auto&& it : gatherCmp.mScannedTiles) {
                    DebugRenderer::drawWireQuad(f32v2(it.getWorldPos2D()), f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 1.0f), SCAN_FRAMES_DELAY);
                }
            }

            // Mark all tiles as reserved
            for (auto&& it : gatherCmp.mScannedTiles) {
                assert(!it.getMutableContainer()->getTileAt(it.tileIndex).hasFlagMainThread(TileFlags::TILE_FLAG_IS_RESOURCE_RESERVED));
                it.getMutableContainer()->setTileFlag(it.tileIndex, TileFlags::TILE_FLAG_IS_RESOURCE_RESERVED);
            }
            break;
        }
    }

    // Assign gather tasks to workers
    if (ownershipCmp.mOwnedStockpiles.size()) {
        while (businessCmp.mIdleWorkers.size() && gatherCmp.mScannedTiles.size()) {
            TileHandle handle = gatherCmp.mScannedTiles.back();

            assert(handle.tile->hasFlagMainThread(TileFlags::TILE_FLAG_IS_RESOURCE_RESERVED));

            entt::entity worker = businessCmp.mIdleWorkers.front();

            EmployeeComponent& employeeCmp = registry.get<EmployeeComponent>(worker);
            assert(!employeeCmp.mCurrentTask);

            ItemStack maximumYieldStack;
            TileLayer gatherLayer;
            if (handle.tile->hasHarvestableResource(gatherCmp.mResourceToGather, &gatherLayer)) {

                const TileData& tileData = TileRepository::getTileData(handle.tile->getLayers()[e_cast(gatherLayer)]);
                // TODO: Play animation of tree falling

                // TODO: HANDLE MULTIPLE DROPS
                for (size_t i = 0; i < tileData.itemDrops.size(); ++i) {
                    const ItemDrop& drop = tileData.itemDrops[i];
                    maximumYieldStack.id = drop.id;
                    maximumYieldStack.quantity = drop.countRange.y;

                    if (std::unique_ptr<ItemReservation> reservation = ownershipCmp.mOwnedStockpiles[0]->tryPromiseItemStack(maximumYieldStack, maximumYieldStack.quantity)) {
                        employeeCmp.mCurrentTask = std::make_unique<GatherTask>(handle, gatherCmp.mResourceToGather, std::move(reservation));
                        break;
                    }
                }
            }
            else {
                std::cout << "Failed to find resource to gather in business cmp";
            }

            // If employee has a task, we succeeded. Otherwise, break cause we cant give any tasks right now
            if (employeeCmp.mCurrentTask) {
                gatherCmp.mScannedTiles.pop_back();
                businessCmp.mIdleWorkers.pop_front();
                employeeCmp.flags &= (~EmployeeComponentFlags::FLAG_EMPLOYEE_IS_IDLE);
                std::cout << "  REMOVE IDLE 2 " << businessCmp.mIdleWorkers.size() << " " << businessCmp.mIdleWorkers.capacity() << std::endl;
            }
            else {
                break;
            }
        }
    }
}

void updateBuildComponent(BusinessBuildComponent& buildCmp, BusinessComponent& businessCmp, entt::entity entity) {
    // Gathering currently requires a city
    assert(businessCmp.mCity);

    // Initialize the job if needed
    if (buildCmp.mCurrentBlueprint && !buildCmp.mCurrentJob) {
        IBusinessJobPtr newJob = std::make_unique<ConstructBuildingJob>(*buildCmp.mCurrentBlueprint);
        buildCmp.mCurrentJob = static_cast<ConstructBuildingJob*>(newJob.get());
        businessCmp.mActiveJobs.push_back(std::move(newJob));
    }
   
}

void updateBusiness(entt::registry& registry, entt::entity entity, BusinessComponent& cmp) {

    OwnershipComponent& ownershipCmp = registry.get<OwnershipComponent>(entity);

    // Check if we need to request a building. If so, we request one, and do nothing else
    if (ownershipCmp.mOwnedPlots.size() == 0) {
        // TODO: Fill out
        PlotRequestProps props;
        City& city = *cmp.mCity;
        CityPlot* plot = city.getCityPlanner().tryPurchasePlot(props, entity);
        if (plot) {
            const BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();
            city.getCityPlanner().generatePlanForPlotAsyncThenSendToBuilder(*plot, cmp.mBusinessDef->mBuildingName, BuildingBlueprintFlags::BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE);
            ownershipCmp.mOwnedPlots.push_back(plot);
        }
        else {
            return;
        }
    }
    
    // Assign idle workers to active jobs
    while (cmp.mIdleWorkers.size()) {
        entt::entity worker = cmp.mIdleWorkers.front();
        bool didAssign = false;
        for (auto&& it : cmp.mActiveJobs) {
            if (IAgentTaskPtr task = it->tryMakeTaskForWorker(worker)) {
                
                EmployeeComponent& employeeCmp = registry.get<EmployeeComponent>(worker);
                employeeCmp.flags &= (~EmployeeComponentFlags::FLAG_EMPLOYEE_IS_IDLE);

                // TODO: Why shared and not unique?
                employeeCmp.mCurrentTask = std::move(task);

                cmp.mIdleWorkers.pop_front();
                std::cout << "  REMOVE IDLE 1 " << cmp.mIdleWorkers.size() << " " << cmp.mIdleWorkers.capacity() << std::endl;
                didAssign = true;
                break;
            }
        }
        // Could not assign any more jobs, break
        // TODO: This might be bad when the task only works for some workers and not others
        if (!didAssign) {
            break;
        }
    }

    BusinessProduceComponent* produceCmp = registry.try_get<BusinessProduceComponent>(entity);
    if (produceCmp) {

    }

    // TODO: More performant to iterate each of these as a list?
    BusinessGatherComponent* gatherCmp = registry.try_get<BusinessGatherComponent>(entity);
    if (gatherCmp) {
        updateGatherComponent(registry, *gatherCmp, cmp, ownershipCmp);
    }

    BusinessBuildComponent* buildCmp = registry.try_get<BusinessBuildComponent>(entity);
    if (buildCmp) {
        updateBuildComponent(*buildCmp, cmp, entity);
    }

    BusinessRetailComponent* retailCmp = registry.try_get<BusinessRetailComponent>(entity);
    if (retailCmp) {

    }

    // Tick active jobs and remove complete
    for (size_t i = 0; i < cmp.mActiveJobs.size();) {
        IBusinessJob& job = *cmp.mActiveJobs[i];
        // Update jobs and remove complete jobs
        if (job.tick(registry, entity)) {
            cmp.mActiveJobs[i] = std::move(cmp.mActiveJobs.back());
            cmp.mActiveJobs.pop_back();
            // TODO: onComplete()?
        }
        else {
            ++i;
        }
    }
}

void BusinessSystem::update(entt::registry& registry)
{

    // TODO: View per component type? Dependencies?
    auto view = registry.view<BusinessComponent>();

    // Update businesses
    for (auto entity : view) {
        auto& cmp = view.get<BusinessComponent>(entity);
        updateBusiness(registry, entity, cmp);
    }

}
