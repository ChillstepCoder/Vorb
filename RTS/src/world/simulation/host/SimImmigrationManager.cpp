#include "stdafx.h"
#include "SimImmigrationManager.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/markup/WorldMarkupGrid.h"
#include "faction/IFactionManager.h"

#include "math/Random.h"

constexpr ui64 MIN_TIME_BETWEEN_IMMIGRATIONS_MS = 4000;

SimImmigrationManager::SimImmigrationManager(HostSimContext& simContext) :
    mHostSimContext(simContext), mWorldAnalytics(simContext.getAnalytics()), mMarkupGrid(simContext.getWorld().getMarkupGrid()) {

}

SimImmigrationManager::~SimImmigrationManager() {

}

void SimImmigrationManager::init() {
    mImmigrationData.reserve(mMarkupGrid.getBodyCount());
    std::vector<std::pair<ui32, f32>> desirabilitiesSort;
    desirabilitiesSort.reserve(256);
    for (BodyID i = 0; i < mMarkupGrid.getBodyCount(); ++i) {
        const WorldBodyMarkupData& bodyMarkup = mMarkupGrid.getBodyData(i);
        if (bodyMarkup.isLand()) {
            BodyImmigrationData& data = mImmigrationData[i];
            data.mPrioritySortedChunks.resize(bodyMarkup.chunks.size());
            desirabilitiesSort.resize(bodyMarkup.chunks.size());
            // Cache priorities and indices for sort
            for (ui32 j = 0; j < bodyMarkup.chunks.size(); ++j) {
                desirabilitiesSort[j].first = j;
                desirabilitiesSort[j].second = mMarkupGrid.getChunkMarkup(bodyMarkup.chunks[j]).settleDesirability;
            }
            // Sort high priority to the end of the stack
            std::sort(desirabilitiesSort.begin(), desirabilitiesSort.end(),
                [](const std::pair<ui32, f32>& first, const std::pair<ui32, f32>& second) -> bool {
                return first.second > second.second;
            });
            // Fill sorted chunks
            for (ui32 j = 0; j < bodyMarkup.chunks.size(); ++j) {
                data.mPrioritySortedChunks[j] = bodyMarkup.chunks[desirabilitiesSort[j].first];
            }
        }
    }
    mImmigrationData.shrink_to_fit();
}

void SimImmigrationManager::tickSimThread(TimestampMs currentTime) {
    const ui64 timeDelta = currentTime - mLastImmigrationTimestamp;

    const SimWorldAnalyticsData& analytics = mWorldAnalytics.getAnalyticsDataSimThread();
    // TODO: More complex
    if (analytics.totalPopulation < analytics.desiredPopulation && timeDelta > MIN_TIME_BETWEEN_IMMIGRATIONS_MS) {
        spawnImmigrationBySea(currentTime);
    }
}

SimImmigrationManager::ImmigrationOrder SimImmigrationManager::getNextImmigrationOrder() {
    ImmigrationOrder rv;
    const SortedBodyMap& bodyMap = mMarkupGrid.getSortedBodies();

    // Favors largest bodies
    constexpr f32 chanceToSpawnPerBody = 0.5f;

    bool spawned = false;
    for (auto&& it = bodyMap.rbegin(); it != bodyMap.rend(); ++it) {
        auto&& immit = mImmigrationData.find(it->second);
        if (immit == mImmigrationData.end()) {
            continue;
        }
        BodyImmigrationData& data = immit->second;
        if (data.mPrioritySortedChunks.size()) {
            if (mHostSimContext.getSimRandomGenerator().getRandomFloatUnsigned() <= chanceToSpawnPerBody) {
                const WorldBodyMarkupData& bodyMarkup = mMarkupGrid.getBodyData(it->second);
                rv.targetChunk = data.mPrioritySortedChunks.back();
                data.mPrioritySortedChunks.pop_back();
                // Find closest edge chunk
                f32 closestDistSq = FLT_MAX;
                const ui32 widthChunks = mHostSimContext.getWidthChunks();
                const f32v2 targetPos = GridIdUtil::getWorldPos(rv.targetChunk, CHUNK_WIDTH, widthChunks);
                for (ChunkID chunkId : bodyMarkup.borderChunks) {
                    const f32v2 chunkPos = GridIdUtil::getWorldPos(chunkId, CHUNK_WIDTH, widthChunks);
                    const f32 distanceSq = glm::length2(targetPos - chunkPos);
                    if (distanceSq < closestDistSq) {
                        rv.startChunk = chunkId;
                        closestDistSq = distanceSq;
                    }
                }
                assert(rv.startChunk != INVALID_CHUNK_ID);
                return rv;
            }
        }
    }

    // TODO: If we get here, we should do another pass with 100% chance?

    return rv;
}

void SimImmigrationManager::spawnImmigrationBySea(TimestampMs currentTime) {
    mLastImmigrationTimestamp = currentTime;

    World& world = mHostSimContext.getWorld();
    IFactionManager& factionManager = world.getFactionManager();
    SimECS& ecs = mHostSimContext.getECS();
    RandomGenerator& gen = mHostSimContext.getSimRandomGenerator();

    FactionID factionId;

    // Get our immigration source and target
    // TODO: Factor in the faction ID and immigration size
    ImmigrationOrder immigrationOrder = getNextImmigrationOrder();
    if (immigrationOrder.startChunk == INVALID_CHUNK_ID) {
        LOG_TRACE("Could not find a valid immigration");
        return;
    }
    const ui32 widthChunks = mHostSimContext.getWidthChunks();
    const f32v2 startPos = GridIdUtil::getWorldPosCenter(immigrationOrder.startChunk, CHUNK_WIDTH, widthChunks);
    const f32v2 targetPos = GridIdUtil::getWorldPosCenter(immigrationOrder.targetChunk, CHUNK_WIDTH, widthChunks);

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

    entt::entity newPeopleIds[MAX_NEW_PEOPLE];

    for (ui32 i = 0; i < numPeople; ++i) {
        newPeopleIds[i] = ecs.createNewPerson(startPos);
    }
    std::span peopleSpan(newPeopleIds, numPeople);

    // Use first entity as leader
    entt::entity groupLeader = ecs.createNewSettlerCaravan(peopleSpan, 0, targetPos);

    factionManager.addEntitiesToFaction(ecs.getRegistrySimThread(), peopleSpan, factionId);

    // TODO: Make group leader special

    // Tell group leader to immigrate to a position

    WorldAnalyticEvent analytic(currentTime, WorldAnalyticEventType::ImmigrationBySea, numPeople);
    mWorldAnalytics.addAnalyticsEvent(analytic);
}
