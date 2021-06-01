#include "stdafx.h"
#include "BusinessComponent.h"

#include "city/City.h"

#include "world/TileScanner.h"

#include "DebugRenderer.h"

const int UPDATE_INTERVAL = 60;

// TODO: Smarter scanning, dont scan same area twice
constexpr int SCAN_FRAMES_DELAY = 3600; // Approx 1 minute
constexpr int MAX_TILES_TO_SCAN_FOR = 64;
constexpr ui32 MAX_SCAN_DISTANCE = 128;
constexpr ui32 MAX_RETURN_TILES = 32;

BusinessSystem::BusinessSystem(World& world) :
    mWorld(world)
{

}

void updateGatherComponent(World& world, BusinessGatherComponent& gatherCmp, BusinessComponent& businessCmp) {
    // Gathering currently requires a city
    assert(businessCmp.mCity);
    
    if (gatherCmp.mScannedTiles.empty()) {
        if (--gatherCmp.mFramesUntilNextScan <= 0) {
            PreciseTimer timer;
            gatherCmp.mFramesUntilNextScan = SCAN_FRAMES_DELAY;
            gatherCmp.mScannedTiles = TileScanner::scanForResource(world, gatherCmp.mResourceToGather, businessCmp.mCity->getCityCenterWorldPos(), MAX_SCAN_DISTANCE, MAX_RETURN_TILES);
            std::cout << " Tile scanning took " << timer.stop() << " ms and returned " << gatherCmp.mScannedTiles.size() << " tiles\n";
            for (auto&& it : gatherCmp.mScannedTiles) {
                DebugRenderer::drawBox(it.getWorldPos(), f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 1.0f), SCAN_FRAMES_DELAY);
            }
        }
    }
    else if (gatherCmp.mFramesUntilNextScan > 0) {
        --gatherCmp.mFramesUntilNextScan;
    }
}

void updateBusiness(World& world, entt::registry& registry, entt::entity entity, BusinessComponent& cmp) {
    BusinessProduceComponent* produceCmp = registry.try_get<BusinessProduceComponent>(entity);
    if (produceCmp) {

    }

    BusinessGatherComponent* gatherCmp = registry.try_get<BusinessGatherComponent>(entity);
    if (gatherCmp) {
        updateGatherComponent(world, *gatherCmp, cmp);
    }

    BusinessRetailComponent* retailCmp = registry.try_get<BusinessRetailComponent>(entity);
    if (retailCmp) {

    }
}

void BusinessSystem::update(entt::registry& registry)
{
    // Update slower
   /* if (--mFramesUntilUpdate <= 0) {
        mFramesUntilUpdate = UPDATE_INTERVAL;
    }
    else {
        return;
    }*/

    // TODO: View per component type? Dependencies?
    auto view = registry.view<BusinessComponent>();

    // Update businesses
    for (auto entity : view) {
        auto& cmp = view.get<BusinessComponent>(entity);
        updateBusiness(mWorld, registry, entity, cmp);
    }

}
