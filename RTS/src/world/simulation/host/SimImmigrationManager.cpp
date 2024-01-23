#include "stdafx.h"
#include "SimImmigrationManager.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "faction/IFactionManager.h"

#include "math/Random.h"

constexpr ui64 MIN_TIME_BETWEEN_IMMIGRATIONS_MS = 4000;

SimImmigrationManager::SimImmigrationManager(HostSimContext& simContext) : mHostSimContext(simContext), mWorldAnalytics(simContext.getAnalytics()) {

}

SimImmigrationManager::~SimImmigrationManager() {

}

void SimImmigrationManager::tickSimThread(TimestampMs currentTime) {
    const ui64 timeDelta = currentTime - mLastImmigrationTimestamp;

    const SimWorldAnalyticsData& analytics = mWorldAnalytics.getAnalyticsDataSimThread();
    // TODO: More complex
    if (analytics.totalPopulation < analytics.desiredPopulation && timeDelta > MIN_TIME_BETWEEN_IMMIGRATIONS_MS) {
        spawnImmigrationBySea(currentTime);
    }
}

void SimImmigrationManager::spawnImmigrationBySea(TimestampMs currentTime) {
    mLastImmigrationTimestamp = currentTime;

    World& world = mHostSimContext.getWorld();
    IFactionManager& factionManager = world.getFactionManager();
    SimECS& ecs = mHostSimContext.getECS();
    RandomGenerator& gen = mHostSimContext.getSimRandomGenerator();

    FactionID factionId;
    
    // TODO: More intelligent probability based on number of existing factions
    if (gen.getRandomBool()) {
        // Try existing faction
        factionId = factionManager.getRandomActiveFactionID();
        if (factionId == INVALID_FACTION_ID) {
            factionId = factionManager.generateRandomNewFaction();
        }
    }
    else {
        // Make new faction
        factionId = factionManager.generateRandomNewFaction();
    }

    constexpr ui32 MIN_NEW_PEOPLE = 1;
    constexpr ui32 MAX_NEW_PEOPLE = 128;
    const ui32 numPeople = gen.getRandomUIntInRange(MIN_NEW_PEOPLE, MAX_NEW_PEOPLE);

    // TODO: Spawn on shore or at appropriate docks
    const ui32 worldWidthTiles = world.getWidthTiles();
    i32v2 spawnPosition;
    spawnPosition.x = (i32)gen.getRandomUIntInRange(128, worldWidthTiles - 128);
    spawnPosition.y = (i32)gen.getRandomUIntInRange(128, worldWidthTiles - 128);

    entt::entity newPeopleIds[MAX_NEW_PEOPLE];

    for (ui32 i = 0; i < numPeople; ++i) {
        newPeopleIds[i] = ecs.createNewPerson(spawnPosition);
    }
    std::span peopleSpan(newPeopleIds, numPeople);

    // TODO: REAL
    i32v2 targetPos(gen.getRandomUIntInRange(2000, 30000), gen.getRandomUIntInRange(2000, 30000));

    // Use first entity as leader
    entt::entity groupLeader = ecs.createNewSettlerCaravan(peopleSpan, 0, targetPos);

    factionManager.addEntitiesToFaction(ecs.getRegistrySimThread(), peopleSpan, factionId);

    // TODO: Make group leader special

    // Tell group leader to immigrate to a position
    //assert(false);

    WorldAnalyticEvent analytic(currentTime, WorldAnalyticEventType::ImmigrationBySea, numPeople);
    mWorldAnalytics.addAnalyticsEvent(analytic);
}
