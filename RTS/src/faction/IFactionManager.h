#pragma once

#include "world/WorldContextObject.h"
#include "faction/Faction.h"

typedef std::pair<FactionID, FactionID> FactionIDPair;
constexpr i8 MAX_RELATION = 100;
constexpr i8 MIN_RELATION = -100;

class IFactionManager : public WorldContextObject
{
public:
    IFactionManager(World& world) : WorldContextObject(world) {};
    virtual ~IFactionManager() = default;

    virtual void addFaction(Faction faction) = 0;
    virtual i8 getFactionRelation(FactionID faction1, FactionID faction2) = 0;

protected:
    FactionIDPair getOrderedFactionIDs(FactionID faction1, FactionID faction2) {
        return faction1 < faction2 ? std::make_pair(faction1, faction2) : std::make_pair(faction2, faction1);
    }
};

