#pragma once
#include "stdafx.h"

#include <Vorb/ecs/ComponentTable.hpp>

#include "item/ArmorItem.h"
#include "item/WeaponItem.h"
#include "item/ShieldItem.h"

struct PhysicsComponent;
struct SimpleSpriteComponent;
class TileGrid;
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

class CombatComponentTable : public vecs::ComponentTable<CombatComponent> {
public:
	static const std::string& NAME;
};

namespace Combat {
	// Return true on defender death
	bool resolveMeleeAttack(CombatComponent& attacker, CombatComponent& defender, PhysicsComponent& defenderPhysComp, SimpleSpriteComponent& defenderSpriteComp, const f32v2& dir, float flankingAngle);
	bool meleeAttackArc(vecs::EntityID source, CombatComponent& attacker, const f32v2& pos, const f32v2& dir, float radius, float arcAngle, TileGrid& world, EntityComponentSystem& ecs);
};