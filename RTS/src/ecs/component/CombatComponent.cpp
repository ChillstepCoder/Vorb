#include "stdafx.h"
#include "CombatComponent.h"
#include "PhysicsComponent.h"
#include "SimpleSpriteComponent.h"
#include "TileGrid.h"
#include "EntityComponentSystem.h"

const std::string& CombatComponentTable::NAME = "combat";

namespace Combat {
	bool resolveMeleeAttack(CombatComponent& attacker, CombatComponent& defender, PhysicsComponent& defenderPhysComp, SimpleSpriteComponent& defenderSpriteComp, const f32v2& dir, float flankingAngle) {
		const WeaponItem& atkWeapon = attacker.mWeapon;
		const ShieldItem& defShield = defender.mShield;
		const ArmorItem& defArmor = defender.mArmor;

		// Base weapon damage
		float damage = WEAPON_BASE_DAMAGES[enum_cast(atkWeapon.mType)] * attacker.mStrength * attacker.mSkill;
		damage *= MATERIAL_QUALITY_MULT[enum_cast(atkWeapon.mMaterial)];

		// Armor
		damage -= ARMOR_BASE_REDUCTION[enum_cast(defArmor.mType)] * MATERIAL_QUALITY_MULT[enum_cast(defArmor.mMaterial)];

		// Damage before shield can never go below 1;
		if (damage < 1.0f) {
			damage = 1.0f;
		}

		// If defender is shielded, reduce damage by weight of shield and defender strength and skill
		if (flankingAngle <= SHIELD_DEFLECTIONS_ANGLES[enum_cast(defShield.mType)]) {
			damage -= SHIELD_WEIGHTS[enum_cast(defShield.mType)]
				* defender.mStrength
				* defender.mSkill
				* MATERIAL_QUALITY_MULT[enum_cast(defShield.mMaterial)];
		}

		if (damage > 0.0f) {
			f32v2 impulse = -dir * 0.05f * damage / 100.0f;
			defenderPhysComp.mBody->ApplyLinearImpulseToCenter(TO_BVEC2_C(impulse), true);
			defender.mHealthRange.x -= damage;
			defenderSpriteComp.hitFlash += 0.7f;
			if (defenderSpriteComp.hitFlash > 1.0f) {
				defenderSpriteComp.hitFlash = 1.0f;
			}
			return defender.mHealthRange.x > 0.0f;
		}
		else {
			return false;
		}
	}

	bool meleeAttackArc(vecs::EntityID source, CombatComponent& attacker, const f32v2& pos, const f32v2& dir, float radius, float arcAngle, TileGrid& world, EntityComponentSystem& ecs) {
		bool wasHit = false;
		// TODO: Team filtering?
		ActorTypesMask includeMask = ~0;
		ActorTypesMask excludeMask = 0;

		std::vector<EntityDistSortKey> entities = world.queryActorsInArc(pos, radius, dir, arcAngle, includeMask, excludeMask, false, 1, source);
		for (auto&& key : entities) {
			wasHit = true;
			vecs::EntityID targetId = key.second;
			PhysicsComponent& targetPhysCmp = ecs.getPhysicsComponentFromEntity(targetId);
			f32v2 dirToEntity = glm::normalize(pos - targetPhysCmp.getPosition());
			float flankingAngle = glm::dot(dir, targetPhysCmp.mDir);
			if (Combat::resolveMeleeAttack(attacker, ecs.getCombatComponentFromEntity(targetId), targetPhysCmp, ecs.getSimpleSpriteComponentFromEntity(targetId), dirToEntity, RAD_TO_DEG(flankingAngle))) {
				ecs.convertEntityToCorpse(targetId);
			}
		}

		return wasHit;
	}
};