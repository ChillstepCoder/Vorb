#pragma once

#include "combat/Attack.h"

#include "tile/TileHandle.h"

class IWorld;


// TODO: Weapon.h

class CombatContext {
public:
    CombatContext(IWorld& world);

    void performAttack(entt::entity source, const AttackData& attackData);

private:
    void performConeAttack(entt::entity source, const AttackData& attackData);
    // void performSphereAttack

    void hitTile(LiteTileHandle liteHandle, ui16v2 damageRange, f32v3 impactPosition, f32v3 impactNormal);

    IWorld& mWorld;
};

