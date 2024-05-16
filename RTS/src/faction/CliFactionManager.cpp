#include "stdafx.h"
#include "CliFactionManager.h"

#include "math/Random.h"
#include "world/World.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

CliFactionManager::CliFactionManager(World& world) : IFactionManager(world) {
    mRandomGenerator = std::make_unique<RandomGenerator>(world.getSeed() ^ (world.getSeed() << 53262));
}

CliFactionManager::~CliFactionManager() = default;

FactionID CliFactionManager::addFaction(Faction faction) {
    {
        std::lock_guard lock(mFactionsMutex);
        mFactions[mNextFactionID] = std::move(faction);
        mActiveFactions.emplace_back(mNextFactionID);
    }
    FactionID rv = mNextFactionID;
    ++mNextFactionID;
    return rv;
}

i8 CliFactionManager::getFactionRelation(FactionID faction1, FactionID faction2) {
    FactionIDPair pair = getOrderedFactionIDs(faction1, faction2);

    {
        std::shared_lock lock(mFactionRelationsMutex);
        auto&& it = mFactionRelations.find(pair);
        if (it != mFactionRelations.end()) {
            return it->second;
        }
    }

    const i8 defaultRelation = getDefaultFactionRelation(pair);
    {
        std::lock_guard lock(mFactionRelationsMutex);
        mFactionRelations[pair] = defaultRelation;
    }
}

void CliFactionManager::addEntitiesToFaction(entt::registry& registry, std::span<entt::entity> entities, FactionID factionId) {

    { // Critical section
        std::lock_guard lock(mFactionsMutex);
        Faction& faction = mFactions[factionId];
        size_t startCopy = faction.members.size();
        faction.members.resize(startCopy + entities.size());
        memcpy(faction.members.data() + startCopy, entities.data(), entities.size_bytes());
    }

    for (entt::entity newEntity : entities) {
        // TODO: Special event on faction switching?
        registry.get_or_emplace<FactionComponent>(newEntity).factionId = factionId;
    }
}

FactionID CliFactionManager::getRandomActiveFactionID() {
    std::shared_lock lock(mFactionsMutex);
    if (mActiveFactions.empty()) [[unlikely]] return INVALID_FACTION_ID;
    if (mActiveFactions.size() == 1) [[unlikely]] return mActiveFactions[0];
    return mActiveFactions[mRandomGenerator->getRandomUIntInRange(0, mActiveFactions.size() - 1)];
}

FactionID CliFactionManager::generateRandomNewFaction() {
    Faction newFaction;
    // TODO: Make this work properly
    return addFaction(newFaction);
}

FactionThreadSafeData CliFactionManager::getFactionThreadSafeData(FactionID factionId) {
    std::lock_guard lock(mFactionsMutex);
    return mFactions[factionId].threadSafeData;
}

i8 CliFactionManager::getDefaultFactionRelation(FactionIDPair factions) {
    BitFlags<FactionTraits> t1, t2;
    {
        std::shared_lock lock(mFactionsMutex);
        auto&& it1 = mFactions.find(factions.x);
        assert(it1 != mFactions.end());
        t1 = it1->second.threadSafeData.traits;
        auto&& it2 = mFactions.find(factions.y);
        assert(it2 != mFactions.end());
        t2 = it2->second.threadSafeData.traits;
    }

    // TODO: Make more complex

    constexpr ui8 DEFAULT_SAMECULT_RELATION = 30;
    constexpr ui8 DEFAULT_INTERCULT_RELATION = -50;

    if (t1.isBitSet(FactionTraits::Player) && t2.isBitSet(FactionTraits::Player)) {
        return MAX_RELATION;
    }
    if ((t1.getBits() & FACTION_TRAIT_ALLEGIANCE_MASK) == (t2.getBits() & FACTION_TRAIT_ALLEGIANCE_MASK)) {
        return DEFAULT_SAMECULT_RELATION;
    }
    if (t1.isBitSet(FactionTraits::BanshiraCult)) {
        if (t2.isBitSet(FactionTraits::ChernobogCult)) {
            return DEFAULT_INTERCULT_RELATION;
        }
        if (t2.isBitSet(FactionTraits::EarthCult)) {
            return MIN_RELATION;
        }
    }
    else if (t1.isBitSet(FactionTraits::ChernobogCult)) {
        if (t2.isBitSet(FactionTraits::BanshiraCult)) {
            return DEFAULT_INTERCULT_RELATION;
        }
        if (t2.isBitSet(FactionTraits::EarthCult)) {
            return MIN_RELATION;
        }
    }
    return 0;
}
