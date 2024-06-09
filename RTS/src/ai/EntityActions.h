#pragma once

class World;
class IFullECS;

namespace EntityActions {

    // Drop bundle
    void dropBundleSim(World& world, entt::registry& simRegistry, entt::entity simAgent);
    void dropBundleFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent);

}