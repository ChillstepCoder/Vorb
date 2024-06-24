#pragma once

#include "world/WorldContextObject.h"
#include "faction/Faction.h"

using FactionIDPair = ui32v2;
constexpr i8 MAX_RELATION = 100;
constexpr i8 MIN_RELATION = -100;

class IFactionManager : public WorldContextObject
{
public:
    IFactionManager(World& world) : WorldContextObject(world) {};
    virtual ~IFactionManager() = default;

    virtual FactionID addFaction(Faction faction) = 0;
    virtual i8 getFactionRelation(FactionID faction1, FactionID faction2) = 0;
    virtual void addEntitiesToFaction(entt::registry& registry, std::span<entt::entity> entities, FactionID faction) = 0;
    virtual FactionID getRandomActiveFactionID() = 0;
    virtual FactionID generateRandomNewFaction() = 0;
    virtual FactionThreadSafeData getFactionThreadSafeData(FactionID factionId) = 0;

protected:
    FactionIDPair getOrderedFactionIDs(FactionID faction1, FactionID faction2) {
        return faction1 < faction2 ? FactionIDPair(faction1, faction2) : FactionIDPair(faction2, faction1);
    }
};

