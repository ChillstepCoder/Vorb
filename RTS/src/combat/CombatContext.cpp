#include "stdafx.h"
#include "CombatContext.h"

#include "world/IWorld.h"

CombatContext::CombatContext(IWorld& world) : mWorld(world) {

}

void CombatContext::performMeleeAttack(entt::entity source, AttackShape shape, f32 radius, f32 angleRad, f32 forwardOffset, BitFlags<AttackFlags> flags) {
    assert(false);
}
