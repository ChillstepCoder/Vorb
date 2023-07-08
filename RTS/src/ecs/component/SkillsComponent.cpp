#include "stdafx.h"
#include "SkillsComponent.h"

KEG_TYPE_DEF_SAME_NAME(SkillsComponentFileData, kt) {
    kt.addValue("skill_names", keg::Value::array(offsetof(SkillsComponentFileData, mSkillNames), keg::BasicType::STRING));
}

void SkillsComponentSystem::update(IWorld& world, entt::registry& registry, f32 elapsedSec) {

	constexpr auto updateSkill =
		[](entt::entity entity, entt::registry& registry, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, f32 elapsedSec) -> bool {
		const SkillDef* skillDef = skillsCmp.mSkills[e_cast(skillsCmp.mActiveSlot)];
		assert(skillDef);
		// Fire all passed triggers
		activeCmp.mElapsed += elapsedSec;
		while (activeCmp.mNextTrigger < skillDef->mNumAttackTriggers) {
			const SkillAttackTrigger& nextTrigger = skillDef->mAttackTriggers[activeCmp.mNextTrigger];
			if (activeCmp.mElapsed >= nextTrigger.mTime) {
				++activeCmp.mNextTrigger;
				// FIRE SKILL TRIGGER
				assert(false);
			}
			else {
				// Not ready to trigger
				break;
			}
		}

		if (activeCmp.mElapsed >= skillDef->mDuration) {
			assert(false);
			// END
		}
	};

	auto view = registry.view<SkillsComponent, ActiveSkillComponent>();
	for (auto entity : view) {
		SkillsComponent& skillsCmp = view.get<SkillsComponent>(entity);
		ActiveSkillComponent& activeCmp = view.get<ActiveSkillComponent>(entity);
		// If skill is finished, remove active cmp
		if (updateSkill(entity, registry, skillsCmp, activeCmp, elapsedSec)) {
			registry.remove<ActiveSkillComponent>(entity);
		}
	};
}

bool SkillsComponentSystem::tryActivateSkillSlot(entt::entity entity, entt::registry& registry, SkillSlot slot) {

	if (registry.try_get<ActiveSkillComponent>(entity)) {
		// TODO: handle interrupts
		// Cannot activate a skill while we are activating a skill
		return false;
	}

	assert(false);
	xxx; // TODO;
	return true;
}

