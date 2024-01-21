#include "stdafx.h"
#include "HostFactionManager.h"


HostFactionManager::HostFactionManager(World& world) : IFactionManager(world), mCliFactionManager(world) {

}

HostFactionManager::~HostFactionManager() = default;

FactionID HostFactionManager::addFaction(Faction faction) {
    // TODO: Replicate to clients
    return mCliFactionManager.addFaction(faction);
}

i8 HostFactionManager::getFactionRelation(FactionID faction1, FactionID faction2) {
    // TODO: Replicate to clients
    return mCliFactionManager.getFactionRelation(faction1, faction2);
}

void HostFactionManager::addEntitiesToFaction(entt::registry& registry, std::span<entt::entity> entities, FactionID faction) {
    mCliFactionManager.addEntitiesToFaction(registry, entities, faction);
}

FactionID HostFactionManager::getRandomActiveFactionID() {
    return mCliFactionManager.getRandomActiveFactionID();
}

FactionID HostFactionManager::generateRandomNewFaction() {
    return mCliFactionManager.generateRandomNewFaction();
}

FactionThreadSafeData HostFactionManager::getFactionThreadSafeData(FactionID factionId) {
    return mCliFactionManager.getFactionThreadSafeData(factionId);
}
