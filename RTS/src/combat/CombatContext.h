#pragma once

#include "combat/Attack.h"

#include "tile/TileHandle.h"
#include "world/WorldContextObject.h"

class World;
class SkillDef;

// TODO: Weapon.h

class CombatContext : public WorldContextObject {
public:
    CombatContext(World& world);

    void performAttack(entt::entity source, const SkillDef& skillDef, const AttackData& attackData);

private:
    void performConeAttack(entt::entity source, const SkillDef& skillDef, const AttackData& attackData);
    // void performSphereAttack

    void hitTile(LiteTileHandle liteHandle, const SkillDef& skillDef, ui16v2 damageRange, f32v3 impactPosition, f32v3 impactNormal, f32v3 impactDir);

};

