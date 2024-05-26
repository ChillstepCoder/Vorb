#include "stdafx.h"
#include "SettlementPlanner.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/system/SimAISystem.h"
#include "world/simulation/host/system/SimSettlementSystem.h"
#include "world/IHeightmapGrid.h"

#include "ai/jobs/ConstructBlueprintSimJob.h"
#include "ai/jobs/SimTaskHandle.h"

#include "building/BuildingRepository.h"
#include "building/BuildingBlueprintGenerator.h"

// OLD SETTLEMENT DESIGN NOTES
//  [SettlementRoadNetwork]
// 1. Define road network, with desired size based on desired number of plots
//    - Starts with a single node and 3 to 4 branches
//  [SettlementDistrictManager]
// 2. Assign districts - basically just connected graphs of ids district ID per tile so they are arbitrary
//    - Districts also have an associated ai controller that manages what should be built inside them, and if they need to expand
//      * If a district cannot expand due to size limitation or needed space, it will try to spawn a new district instance
//  [PlotManager]+[PlotCarver]
// 3. Districts generate plots inside them by carving the available space when buildings are requested.
//    - Plots are generated as needed, rather than precomputed, so the player can easily define his own plots.
//      * District plotter generates plots.
//    - PLOTTER ALGORITHM:
//      a. All roads generate a list of free edge nodes. Edge nodes are free if they are not owned by a plot.
//      b. Plots are extruded from edge nodes by walking the free space, choosing a direction, and finding an AABB that fits along that space.
//      c. AABB fills in blocks towards the road and are included in the free space.
//      d. Plot footprint does not have to fill the AABB. It can have plot tiles that are unowned, which can be replaced by other buildings.
//         - This helps keep things from looking too boxy
//      e. Alleyways are generated (See below)
//      f. Plots cannot block road endcaps. Endcaps are quite long, to prevent buildings from blocking them during other district growth.
// [RoadExtender]
// 4. Road growth
//    - When a building cannot fit in any district, roads will attempt to extend.
//    - Roads always keep ahead of the building tail, to help keep them from cutting each other off. Road cap minimum X tile distance from nearest plot.
//    - When roads extend they grow in one of two ways
//    a. Lengthen the road, usually in a straight line, but with raycast if straight line is occluded, we can attempt to bend the road. (Spline?)
//    b. Branch - Create a fork, T-junction, or X-junction. Configurable shape based on the settlement type.
//    - Roads can collide with each other, and will attempt to do so with pathfinding to create circles. (Needs elaboration)


SettlementPlanner::SettlementPlanner(World& world, SimECS& simEcs, entt::registry& registry) : mWorld(world), mEcs(simEcs), mRegistry(registry) {

}

void SettlementPlanner::onSettlementCreated(entt::entity settlementEntity, TimestampMs currentTime) {
    ASSERT_SIM_THREAD();
    mLastThinkTime = currentTime;

    SettlementSimComponent& simCmp = mRegistry.get<SettlementSimComponent>(settlementEntity);
    TileCoord worldPosCenter = mWorld.getChunkWorldPos(simCmp.rootChunkId) + TileCoord(CHUNK_WIDTH / 2);

    // Create roads
    SettlementLayoutComponent& layoutCmp = mRegistry.get<SettlementLayoutComponent>(settlementEntity);
    layoutCmp.manager.tryInitAtWorldPos(mWorld, settlementEntity, DTileCoord(worldPosCenter));
}

void SettlementPlanner::updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime) {
    ASSERT_SIM_THREAD();

    mLastThinkTime = currentTime; // TODO: Timestep?

    //SettlementSimComponent& simCmp = mRegistry.get<SettlementSimComponent>(settlementEntity);
    //SettlementQuartermasterComponent& quartermasterCmp = mRegistry.get<SettlementQuartermasterComponent>(settlementEntity);
    //SettlementPeopleComponent& peopleCmp = mRegistry.get<SettlementPeopleComponent>(settlementEntity);

    updateResidentsPendingHomes(settlementEntity);

}

void SettlementPlanner::updateResidentsPendingHomes(entt::entity settlementEntity) {
    SimAISystem& aiSystem = mEcs.getAISystem();
    SimSettlementSystem& settlementSystem = mEcs.getSettlementSystem();

    BuildingRepository& buildingRepo = BuildingRepository::get();
    const BuildingDef& houseDef = buildingRepo.getLoadedOrUnloadedAsset(CStrToken("small_house"));

    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();

    SettlementPlannerComponent& plannerCmp = mRegistry.get<SettlementPlannerComponent>(settlementEntity);
    SettlementLayoutComponent& layoutCmp = mRegistry.get<SettlementLayoutComponent>(settlementEntity);

    // Helper
    auto makeHomePlotForCharacters = [&](entt::entity* characters, i32 numCharacters) -> bool {
        // TODO: Preferences
        SettlementPlotRequest request;
        request.allowedZones = BitFlags<SettlementZone>(SettlementZone::UrbanResidential, SettlementZone::Rural);

        SettlementPlotID plotId = layoutCmp.manager.tryClaimOrGeneratePlot(request, characters[0], true);
        if (plotId != INVALID_SETTLEMENT_PLOT_ID) {
            settlementSystem.getCharacterInterface().makePlotOwnedByEntity(characters[0], plotId);

            SettlementPlot& newPlot = layoutCmp.manager.getPlot(plotId);
            const f32 zApprox = heightGrid.getHeightAtVert<true>(DTileCoord(newPlot.aabbDTile.pos + newPlot.aabbDTile.dims / 2));
            // TODO: Correct cartesian!
            newPlot.activeBlueprint = BuildingBlueprintGenerator::tryGenerateBlueprintSynchronous(houseDef, 1.0f /*?*/, Cartesian::WEST, DTileCoord(newPlot.aabbDTile.pos), newPlot.aabbDTile.dims, newPlot.ownedDTiles, BuildingBlueprintFlags(0), Random::getCachedRandom(), zApprox);
            if (newPlot.activeBlueprint) {
                newPlot.activeBlueprint->assignToSettlement(settlementEntity, plotId);

                std::unique_ptr<ConstructBlueprintSimJob> newConstructJob = std::make_unique<ConstructBlueprintSimJob>(*newPlot.activeBlueprint, mEcs, characters[0]);
                SimJobBossComponent& jobBossCmp = mRegistry.get_or_emplace<SimJobBossComponent>(characters[0]);

                // Instruct all characters to build this house
                for (int i = 0; i < numCharacters; ++i) {
                    mRegistry.get<SimResidentComponent>(characters[i]).homeState = SimHomeState::Building;
                    // TODO: Handle switching jobs
                    SimTaskQueueComponent& taskQueue = mRegistry.get<SimTaskQueueComponent>(characters[i]);
                    if (taskQueue.taskQueue.size() < MAX_SIM_TASK_QUEUE_SIZE) {
                        taskQueue.taskQueue.push_back(SimTaskHandle(newConstructJob.get(), characters[i]));
                        taskQueue.taskQueue.back().init();
                    }
                }

                // Track job
                jobBossCmp.activeJobs.emplace_back(std::move(newConstructJob));

            }
            else {
                for (int i = 0; i < numCharacters; ++i) {
                    // TODO: Handle this failure case
                    mRegistry.get<SimResidentComponent>(characters[i]).homeState = SimHomeState::NeedsBlueprint;
                }
            }
            return true;
        }
        return false;
    };

    // Prioritize families
    if (plannerCmp.familiesPendingHomes.size()) {
        FamilyID pendingFamily = plannerCmp.familiesPendingHomes[0];
        SimFamily& family = aiSystem.getFamily(pendingFamily);

        if (makeHomePlotForCharacters(family.characters.get(), family.numCharacters)) {

            // This is a slow operation but this array is small so its fine
            plannerCmp.familiesPendingHomes.erase(plannerCmp.familiesPendingHomes.begin());
        }
    }
    else if (plannerCmp.singleCharactersPendingHomes.size()) {
        entt::entity character = plannerCmp.singleCharactersPendingHomes[0];

        if (makeHomePlotForCharacters(&character, 1)) {

            // This is a slow operation but this array is small so its fine
            plannerCmp.singleCharactersPendingHomes.erase(plannerCmp.singleCharactersPendingHomes.begin());
        }
    }
}
