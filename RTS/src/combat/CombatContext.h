#pragma once

#include "combat/Attack.h"

#include "tile/TileHandle.h"

class World;


// TODO: Weapon.h

class CombatContext {
public:
    CombatContext(World& world);

    void performAttack(entt::entity source, const AttackData& attackData);

private:
    void performConeAttack(entt::entity source, const AttackData& attackData);
    // void performSphereAttack

    void hitTile(LiteTileHandle liteHandle, ui16v2 damageRange, f32v3 impactPosition, f32v3 impactNormal);

    World& mWorld;
};

