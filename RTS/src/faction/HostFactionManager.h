#pragma once

#include "faction/IFactionManager.h"
#include "faction/CliFactionManager.h"


class HostFactionManager : public IFactionManager
{
public:
    HostFactionManager(World& world);
    ~HostFactionManager();

    void addFaction(Faction faction) override;
    i8 getFactionRelation(FactionID faction1, FactionID faction2) override;
private:
    // Host is mainly a replication layer on top of client behavior
    CliFactionManager mCliFactionManager;
};

