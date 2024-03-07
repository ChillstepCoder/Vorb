#include "stdafx.h"
#include "SettlementPlanner.h"

#include "world/World.h"
#include "world/simulation/host/component/SettlementComponents.h"

// Cart bones
// root
//   axle_f
//     wheel_fl
//     wheel_fr
//   wheel_bl
//   wheel_br
//   yoke
//  - All wheel joints should be oriented exactly the same

// NEW SETTLEMENT DESIGN
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


SettlementPlanner::SettlementPlanner(World& world, entt::registry& registry) : mWorld(world), mRegistry(registry) {

}

void SettlementPlanner::onSettlementCreated(entt::entity settlementEntity, TimestampMs currentTime) {
    mLastThinkTime = currentTime;

    SettlementSimComponent& simCmp = mRegistry.get<SettlementSimComponent>(settlementEntity);
    TileCoord worldPosCenter = mWorld.getChunkWorldPos(simCmp.rootChunkId) + TileCoord(CHUNK_WIDTH / 2);

    // Create roads
    SettlementLayoutComponent& roadCmp = mRegistry.get<SettlementLayoutComponent>(settlementEntity);
    roadCmp.manager.tryInitAtWorldPos(mWorld, settlementEntity, DTileCoord(worldPosCenter));
}

void SettlementPlanner::updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime) {
    mLastThinkTime = currentTime; // TODO: Timestep?

    SettlementSimComponent& simCmp = mRegistry.get<SettlementSimComponent>(settlementEntity);
    //SettlementPlannerComponent& plannerCmp = mRegistry.get<SettlementPlannerComponent>(settlementEntity);
    SettlementQuartermasterComponent& quartermasterCmp = mRegistry.get<SettlementQuartermasterComponent>(settlementEntity);
    SettlementPeopleComponent& peopleCmp = mRegistry.get<SettlementPeopleComponent>(settlementEntity);
    SettlementStructuresComponent& structuresCmp = mRegistry.get<SettlementStructuresComponent>(settlementEntity);

    //SettlementWorkOrdersComponent& workOrdersCmp = mRegistry.get<SettlementWorkOrdersComponent>(settlementEntity);


}
