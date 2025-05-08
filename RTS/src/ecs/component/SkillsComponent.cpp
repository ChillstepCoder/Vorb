#include "stdafx.h"
#include "SkillsComponent.h"

#include "world/World.h"
#include "combat/CombatContext.h"

// For asset handle
#include "resources/IAssetRepository.h"

void SkillsComponentSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    ASSERT_GAME_THREAD();

	const auto updateSkill =
		[&](entt::entity entity, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, f32 elapsedSec) -> bool {
		const SkillDef* skillDef = skillsCmp.mSkills[e_cast(skillsCmp.mActiveSlot)]->tryGetLoadedAsset();
		if (!skillDef) return false;
		// Fire all passed triggers
		activeCmp.mElapsed += elapsedSec;
		while (activeCmp.mNextTrigger < skillDef->mNumTriggers) {
			const SkillTrigger& nextTrigger = skillDef->mTriggers[activeCmp.mNextTrigger];
			if (activeCmp.mElapsed >= nextTrigger.mTime) {
				++activeCmp.mNextTrigger;
				handleSkillTrigger(world, entity, skillsCmp, activeCmp, nextTrigger);
			}
			else {
				// Not ready to trigger
				break;
			}
		}

		if (activeCmp.mElapsed >= skillDef->mDuration) {
			dispatchEnd(SkillEvent{ .mEntity = entity, .mSkillSlot = skillsCmp.mActiveSlot });
			skillsCmp.mActiveSlot = SkillSlot::NONE;
			return true;
		}
		return false;
	};

	auto view = registry.view<SkillsComponent, ActiveSkillComponent>();
	for (auto entity : view) {
		SkillsComponent& skillsCmp = view.get<SkillsComponent>(entity);
		ActiveSkillComponent& activeCmp = view.get<ActiveSkillComponent>(entity);
		// If skill is finished, remove active cmp
		if (updateSkill(entity, skillsCmp, activeCmp, elapsedSec)) {
			registry.remove<ActiveSkillComponent>(entity);
		}
	};
}

bool SkillsComponentSystem::tryActivateSkillSlot(entt::entity entity, entt::registry& registry, SkillSlot slot) {
	ASSERT_GAME_THREAD();
	assert(slot != SkillSlot::NONE);

	if (registry.try_get<ActiveSkillComponent>(entity)) {
		// TODO: handle interrupts
		// Cannot activate a skill while we are activating a skill
		return false;
	}

	SkillsComponent* skillsCmp = registry.try_get<SkillsComponent>(entity);
	if (!skillsCmp) {
		return false;
	}

	const SkillDef* skillDef = skillsCmp->mSkills[e_cast(slot)]->tryGetLoadedAsset();
	if (!skillDef) {
		LOG_WARN("Tried to activate skill that is not loadedL {}", skillsCmp->mSkills[e_cast(slot)]->getDescriptor().getName().toString());
		return false;
	}

	skillsCmp->mActiveSlot = slot;

	ActiveSkillComponent& activeCmp = registry.emplace<ActiveSkillComponent>(entity);
	activeCmp.mDef = skillDef;

	// Event
	dispatchActivate(SkillEvent{ .mEntity = entity, .mSkillSlot = slot });
	return true;
}

void SkillsComponentSystem::handleSkillTrigger(World& world, entt::entity entity, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, const SkillTrigger& trigger) {
	switch (trigger.mType) {
		case SkillTriggerType::Simple:
			assert(false);
			break;
		case SkillTriggerType::Attack: {
			handleAttackTrigger(world, entity, activeCmp, trigger.mAttackTrigger);
            break;
        }
		default:
			assert(false);
			break;

	}
}

void SkillsComponentSystem::handleAttackTrigger(World& world, entt::entity entity, ActiveSkillComponent& activeCmp, const SkillAttackTrigger& attackTrigger) {
	AttackData attackData;
	switch (attackTrigger.mShape) {
		case AttackShape::SPHERE:
			assert(false);
		case AttackShape::CONE:
			// TODO: Forward offset
			attackData.initAsCone(attackTrigger.mRadius, attackTrigger.mAngle, 0.0f, attackTrigger.mHeight, attackTrigger.mDamageRange);
			break;
		default:
			assert(false);
	}
    static_assert(e_count(AttackShape) == 2);
    attackData.swingDir = attackTrigger.mSwingDir;
	attackData.swingHeight = attackTrigger.mSwingHeight;
	world.getCombatContext().performAttack(entity, *activeCmp.mDef, attackData);
}

