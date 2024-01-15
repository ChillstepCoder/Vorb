#include "stdafx.h"
#include "HostFactionManager.h"


HostFactionManager::HostFactionManager(World& world) : IFactionManager(world), mCliFactionManager(world)
{

}

HostFactionManager::~HostFactionManager() = default;

void HostFactionManager::addFaction(Faction faction) {
    // TODO: Replicate to clients
    mCliFactionManager.addFaction(faction);
}

i8 HostFactionManager::getFactionRelation(FactionID faction1, FactionID faction2) {
    // TODO: Replicate to clients
    return mCliFactionManager.getFactionRelation(faction1, faction2);
}
