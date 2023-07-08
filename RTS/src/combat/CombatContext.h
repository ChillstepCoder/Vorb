#pragma once

#include "combat/Attack.h"

class IWorld;

enum class AttackFlags : ui8 {
    //CanHitTree
};

// TODO: Weapon.h

class CombatContext {
public:
    CombatContext(IWorld& world);

    void performMeleeAttack(entt::entity source, AttackShape shape, f32 radius, f32 angleRad, f32 forwardOffset, BitFlags<AttackFlags> flags);

private:
    IWorld& mWorld;
};

