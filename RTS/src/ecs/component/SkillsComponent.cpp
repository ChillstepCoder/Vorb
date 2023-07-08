#include "stdafx.h"
#include "SkillsComponent.h"

#include "world/IWorld.h"
#include "combat/CombatContext.h"

KEG_TYPE_DEF_SAME_NAME(SkillsComponentFileData, kt) {
    kt.addValue("skill_names", keg::Value::array(offsetof(SkillsComponentFileData, mSkillNames), keg::BasicType::STRING));
}

void SkillsComponentSystem::update(IWorld& world, entt::registry& registry, f32 elapsedSec) {
    ASSERT_GAME_THREAD();

	const auto updateSkill =
		[&](entt::entity entity, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, f32 elapsedSec) -> bool {
		const SkillDef* skillDef = skillsCmp.mSkills[e_cast(skillsCmp.mActiveSlot)];
		assert(skillDef);
		// Fire all passed triggers
		activeCmp.mElapsed += elapsedSec;
		while (activeCmp.mNextTrigger < skillDef->mNumTriggers) {
			const SkillTrigger& nextTrigger = skillDef->mTriggers[activeCmp.mNextTrigger];
			if (activeCmp.mElapsed >= nextTrigger.mTime) {
				++activeCmp.mNextTrigger;
				handleSkillTrigger(world, entity, skillsCmp, activeCmp, nextTrigger);
				// FIRE SKILL TRIGGER
				assert(false);
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

	if (registry.try_get<ActiveSkillComponent>(entity)) {
		// TODO: handle interrupts
		// Cannot activate a skill while we are activating a skill
		return false;
	}

	SkillsComponent* skillsCmp = registry.try_get<SkillsComponent>(entity);
	if (!skillsCmp) {
		return false;
	}

	skillsCmp->mActiveSlot = slot;

	registry.emplace<ActiveSkillComponent>(entity);

	// Event
	dispatchActivate(SkillEvent{ .mEntity = entity, .mSkillSlot = slot });
	return true;
}

void SkillsComponentSystem::handleSkillTrigger(IWorld& world, entt::entity entity, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, const SkillTrigger& trigger) {
	switch (trigger.mType) {
		case SkillTriggerType::Simple:
			assert(false);
			break;
		case SkillTriggerType::Attack: {
			world.getCombatContext().performMeleeAttack(entity, trigger.mAttackTrigger.mShape, trigger.mAttackTrigger.mRadius, trigger.mAttackTrigger.mAngle, 0.0f, 0);
            break;
        }
		default:
			assert(false);
			break;

	}
}

