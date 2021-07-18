#pragma once

#include "item/ArmorItem.h"
#include "item/WeaponItem.h"
#include "item/ShieldItem.h"

class PhysicsComponent;
class SimpleSpriteComponent;
class World;
class EntityComponentSystem;

struct CombatComponent {
	f32v2 mHealthRange = f32v2(100.0f);
	f32v2 mManaRange = f32v2(0.0f);
	float mStrength = 1.0f;
	float mAgility = 1.0f;
	float mSkill = 1.0f;
	float mMana = 0.0f;
	ArmorItem mArmor;
	WeaponItem mWeapon;
	ShieldItem mShield;
};

namespace Combat {
	// Return true on defender death
	bool resolveMeleeAttack(CombatComponent& attacker, CombatComponent& defender, PhysicsComponent& defenderPhysComp, SimpleSpriteComponent& defenderSpriteComp, const f32v2& dir, float flankingAngle);
	bool meleeAttackArc(entt::entity source, CombatComponent& attacker, const f32v2& pos, const f32v2& dir, float radius, float arcAngle, World& world, EntityComponentSystem& ecs);
};