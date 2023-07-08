#pragma once

#include "definitions/SkillDef.h"

class IWorld;

struct SkillsComponentFileData {
    Array<nString> mSkillNames;
};
KEG_TYPE_DECL(SkillsComponentFileData);

enum class SkillSlot {
    Primary,
    Secondary,
    NONE,
    COUNT = NONE
};

struct ActiveSkillComponent {
    f32 mElapsed = 0.0f;
    int mNextTrigger = 0;
};

struct SkillsComponent {
    // TODO: Not vector
    std::vector<const SkillDef*> mSkills;
    SkillSlot mActiveSlot = SkillSlot::NONE;
};

class SkillsComponentSystem {
public:
    void update(IWorld& world, entt::registry& registry, f32 elapsedSec);

    bool tryActivateSkillSlot(entt::entity entity, entt::registry& registry, SkillSlot slot);
}
//static_assert(sizeof(SkillsComponent) == 32, "Shrink this later");

