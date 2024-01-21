#pragma once

#include "faction/IFactionManager.h"
#include "faction/CliFactionManager.h"


class HostFactionManager : public IFactionManager
{
public:
    HostFactionManager(World& world);
    ~HostFactionManager();

    FactionID addFaction(Faction faction) override;
    i8 getFactionRelation(FactionID faction1, FactionID faction2) override;
    void addEntitiesToFaction(entt::registry& registry, std::span<entt::entity> entities, FactionID faction) override;
    FactionID getRandomActiveFactionID() override;
    FactionID generateRandomNewFaction() override;
    FactionThreadSafeData getFactionThreadSafeData(FactionID factionId) override;
private:
    // Host is mainly a replication layer on top of client behavior
    CliFactionManager mCliFactionManager;
};

