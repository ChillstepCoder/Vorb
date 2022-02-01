#include "stdafx.h"
#include "BusinessComponent.h"

#include "city/City.h"
#include "city/CityBuilder.h"
#include "city/BuildingBlueprint.h"

#include "World.h"
#include "ResourceManager.h"
#include "city/BuildingDescriptionRepository.h"
#include "city/business_jobs/ConstructBuildingJob.h"
#include "world/TileScanner.h"

#include "DebugRenderer.h"
#include "options/DebugOptions.h"

#include "city/CityPlanner.h"

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
    mIdleWorkers.push_back(worker);
}

BusinessSystem::BusinessSystem(World& world) :
    mWorld(world)
{
}

// TODO: Rename to task type?
enum TaskPriorities {
    TASK_PRIORITY_RETAIL,
    TASK_PRIORITY_BUILD,
    TASK_PRIORITY_GATHER,
};

void updateGatherComponent(entt::registry& registry, World& world, BusinessGatherComponent& gatherCmp, BusinessComponent& businessCmp) {
    // Gathering currently requires a city
    assert(businessCmp.mCity);
    
    // Scans
    if (gatherCmp.mScannedTiles.empty()) {
        PreciseTimer timer;
        gatherCmp.mScannedTiles = TileScanner::scanForResource(world, gatherCmp.mResourceToGather, businessCmp.mCity->getCityCenterWorldPos(), MAX_SCAN_DISTANCE, MAX_RETURN_TILES);
        std::cout << " Tile scanning took " << timer.stop() << " ms and returned " << gatherCmp.mScannedTiles.size() << " tiles\n";
        if (sDebugOptions.mShowPaths) {
            for (auto&& it : gatherCmp.mScannedTiles) {
                DebugRenderer::drawWireQuad(it.getWorldPos(), f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 1.0f), SCAN_FRAMES_DELAY);
            }
        }

        // Mark all tiles as reserved
        for (auto&& it : gatherCmp.mScannedTiles) {
            it.getMutableChunk()->setTileFlag(it.index, TILE_FLAG_IS_RESOURCE_RESERVED);
        }
    }

    // Assign gather tasks to workers
    while (businessCmp.mIdleWorkers.size() && gatherCmp.mScannedTiles.size()) {
        TileHandle handle = gatherCmp.mScannedTiles.back();
        gatherCmp.mScannedTiles.pop_back();

        entt::entity worker = businessCmp.mIdleWorkers.front();
        businessCmp.mIdleWorkers.pop_front();

        EmployeeComponent& employeeCmp = registry.get<EmployeeComponent>(worker);
        employeeCmp.flags &= (~EmployeeComponentFlags::FLAG_EMPLOYEE_IS_IDLE);

        // TODO: Why shared and not unique?
        employeeCmp.mCurrentTask = std::make_shared<GatherTask>(handle, gatherCmp.mResourceToGather, businessCmp.mCity);
    }
}

void updateBuildComponent(World& world, BusinessBuildComponent& buildCmp, BusinessComponent& businessCmp, entt::entity entity) {
    // Gathering currently requires a city
    assert(businessCmp.mCity);

    // Initialize the job if needed
    if (buildCmp.mCurrentBlueprint && !buildCmp.mCurrentJob) {
        IBusinessJobPtr newJob = std::make_unique<ConstructBuildingJob>(buildCmp.mCurrentBlueprint);
        buildCmp.mCurrentJob = static_cast<ConstructBuildingJob*>(newJob.get());
        businessCmp.mActiveJobs.push_back(std::move(newJob));
    }
   
}

void updateBusiness(World& world, entt::registry& registry, entt::entity entity, BusinessComponent& cmp) {

    // Check if we need to request a building. If so, we request one, and do nothing else
    if (cmp.mOwnedPlots.size() == 0) {
        // TODO: Fill out
        PlotRequestProps props;
        City& city = *cmp.mCity;
        CityPlot* plot = city.getCityPlanner().tryPurchasePlot(props);
        if (plot) {
            const BuildingDescriptionRepository& buildingRepo = world.getResourceManager().getBuildingRepository();
            city.getCityPlanner().generatePlanForPlotAsyncThenSendToBuilder(*plot, "lumbermill");
            cmp.mOwnedPlots.push_back(plot);
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
            if (it->canAssignWorker()) {
                it->assignWorker(worker);
                cmp.mIdleWorkers.pop_front();
                didAssign = true;
                break;
            }
        }
        // Could not assign any more jobs, break
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
        updateGatherComponent(registry, world, *gatherCmp, cmp);
    }

    BusinessBuildComponent* buildCmp = registry.try_get<BusinessBuildComponent>(entity);
    if (buildCmp) {
        updateBuildComponent(world, *buildCmp, cmp, entity);
    }

    BusinessRetailComponent* retailCmp = registry.try_get<BusinessRetailComponent>(entity);
    if (retailCmp) {

    }

    // Tick active jobs and remove complete
    for (size_t i = 0; i < cmp.mActiveJobs.size();) {
        IBusinessJob& job = *cmp.mActiveJobs[i];
        // Update jobs and remove complete jobs
        if (job.tick(world, registry, entity)) {
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
        updateBusiness(mWorld, registry, entity, cmp);
    }

}
