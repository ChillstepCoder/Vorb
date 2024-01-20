#pragma once
#include "faction/IFactionManager.h"

#include <shared_mutex>

class RandomGenerator;

class CliFactionManager : public IFactionManager
{
    friend class HostFactionManager;
public:
    CliFactionManager(World& world);
    ~CliFactionManager();

    FactionID addFaction(Faction faction) override; // TODO: Host driven
    i8 getFactionRelation(FactionID faction1, FactionID faction2) override;
    void addEntitiesToFaction(entt::registry& registry, std::span<entt::entity> entities, FactionID factionId) override;
    FactionID getRandomActiveFactionID() override;
    FactionID generateRandomNewFaction() override;
private:
    i8 getDefaultFactionRelation(FactionIDPair factions);

    std::shared_mutex mFactionsMutex;
    std::unordered_map<FactionID, Faction> mFactions;
    std::vector<FactionID> mActiveFactions;
    std::unique_ptr<RandomGenerator> mRandomGenerator;

    std::shared_mutex mFactionRelationsMutex;
    std::unordered_map<FactionIDPair, i8> mFactionRelations; //[-100, 100]
    FactionID mNextFactionID = 0; // TODO: Serialize this / Control on host?
};

