#include "stdafx.h"
#include "CliFactionManager.h"

void CliFactionManager::addFaction(Faction faction) {
    {
        std::lock_guard lock(mFactionsMutex);
        mFactions[mNextFactionID] = std::move(faction);
    }
    ++mNextFactionID;
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


i8 CliFactionManager::getDefaultFactionRelation(FactionIDPair factions) {
    BitFlags<FactionTraits> t1, t2;
    {
        std::shared_lock lock(mFactionsMutex);
        auto&& it1 = mFactions.find(factions.first);
        assert(it1 != mFactions.end());
        t1 = it1->second.traits;
        auto&& it2 = mFactions.find(factions.second);
        assert(it2 != mFactions.end());
        t2 = it2->second.traits;
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
