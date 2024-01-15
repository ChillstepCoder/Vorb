#pragma once
#include "faction/IFactionManager.h"

#include <shared_mutex>

class CliFactionManager : public IFactionManager
{
    friend class HostFactionManager;
public:
    CliFactionManager(World& world);
    ~CliFactionManager();

    void addFaction(Faction faction) override; // TODO: Host driven
    i8 getFactionRelation(FactionID faction1, FactionID faction2) override;
private:
    i8 getDefaultFactionRelation(FactionIDPair factions);

    std::shared_mutex mFactionsMutex;
    std::unordered_map<FactionID, Faction> mFactions;

    std::shared_mutex mFactionRelationsMutex;
    std::unordered_map<FactionIDPair, i8> mFactionRelations; //[-100, 100]
    FactionID mNextFactionID = 0; // TODO: Serialize this / Control on host?
};

