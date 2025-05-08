#include "stdafx.h"
#include "SimImmigrationManager.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/markup/WorldMarkupGrid.h"
#include "world/ownership/OwnershipGrid.h"
#include "faction/IFactionManager.h"

#include "world/simulation/host/system/SimAISystem.h"

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
    for (WorldBodyID i = 0; i < mMarkupGrid.getBodyCount(); ++i) {
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
                return first.second < second.second;
            });
            // Fill sorted chunks
            for (ui32 j = 0; j < bodyMarkup.chunks.size(); ++j) {
                data.mPrioritySortedChunks[j] = bodyMarkup.chunks[desirabilitiesSort[j].first];
            }
        }
    }
    // TODO: Evaluate rehashing
    mImmigrationData.shrink_to_fit();
}

void SimImmigrationManager::tickSimThread(TimestampMs currentTime) {
    PROFILE_FUNCTION();
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
    OwnershipGrid& ownershipGrid = mHostSimContext.getWorld().getOwnershipGrid();
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
                if (ownershipGrid.isChunkIsClaimed(rv.targetChunk)) {
                    continue;
                }
                // Find closest edge chunk
                f32 closestDistSq = FLT_MAX;
                const ui32 widthChunks = mHostSimContext.getWidthChunks();
                const f32v2 targetPos = GridIdUtil::getWorldPos(rv.targetChunk, CHUNK_WIDTH, widthChunks);
                for (ChunkID chunkId : bodyMarkup.borderChunks) {
                    const f32v2 chunkPos = GridIdUtil::getWorldPos(chunkId, CHUNK_WIDTH, widthChunks);
                    f32 distanceSq = glm::length2(targetPos - chunkPos);
                    const WorldBodyID adjacentBodyID = mMarkupGrid.getChunkMarkup(chunkId).mainWaterBodyID;
                    if (adjacentBodyID == InvalidWOrldBodyID || mMarkupGrid.getBodyData(adjacentBodyID).bodyType != WorldMarkupBodyType::Ocean) {  
                        // We don't want to spawn on lakes, invalid should be impossible but IDK
                        distanceSq += SQ(32768.f);
                    }
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
    mHostSimContext.getWorld().getOwnershipGrid().claimChunk(immigrationOrder.targetChunk);

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

    constexpr i32 MIN_NEW_PEOPLE = 1;
    constexpr i32 MAX_NEW_PEOPLE = 128;
    const i32 numPeople = gen.getRandomIntInRange(MIN_NEW_PEOPLE, MAX_NEW_PEOPLE);

    entt::entity newPeopleIds[MAX_NEW_PEOPLE];

    // Create all characters
    for (i32 i = 0; i < numPeople; ++i) {
        newPeopleIds[i] = ecs.createNewPerson(startPos);
    }

    // Create a bunch of random families from the characters
    // TODO: Do better here
    SimAISystem& aiSystem = ecs.getAISystem();
    constexpr i32 MAX_PEOPLE_IN_FAMILY = 2;
    for (i32 i = 0; i < numPeople;) {
        const i32 familySize = glm::min(gen.getRandomIntInRange(1, MAX_PEOPLE_IN_FAMILY + 1), numPeople - i);

        if (familySize > 1) {
            std::span<entt::entity> family(&newPeopleIds[i], familySize);
            aiSystem.createFamily(family, "New family"); // TODO: Family name
        }

        i += familySize;
    }
    

    std::span peopleSpan(newPeopleIds, numPeople);

    // Use first entity as leader
    entt::entity groupLeader = ecs.createNewSettlerCaravan(peopleSpan, 0, immigrationOrder.targetChunk);

    factionManager.addEntitiesToFaction(ecs.getRegistrySimThread(), peopleSpan, factionId);

    // TODO: Make group leader special

    // Tell group leader to immigrate to a position

    WorldAnalyticEvent analytic(currentTime, WorldAnalyticEventType::ImmigrationBySea, numPeople);
    mWorldAnalytics.addAnalyticsEvent(analytic);
}
