#pragma once

#include <Vorb/Event.hpp>
#include "definitions/SkillDef.h"

#include "events/SkillEvent.h"
#include "ecs/component/ComponentDefBase.h"

class World;

class SkillsComponentDef : public ComponentDefBase {
public:
    // TODO: AssetRef
    std::vector<nString> mSkillNames;
};
SERIALIZABLE_SIMPLE(SkillsComponentDef,
    make_field(o.mSkillNames, "skill_names"sv)
)

struct ActiveSkillComponent {
    const SkillDef* mDef = nullptr;
    f32 mElapsed = 0.0f;
    int mNextTrigger = 0;
};

struct SkillsComponent {
    // TODO: Not vector
    std::vector<AssetHandlePtr<SkillDef>> mSkills;
    SkillSlot mActiveSlot = SkillSlot::NONE;
};

class SkillsComponentSystem {
public:
    void update(World& world, entt::registry& registry, f32 elapsedSec);

    bool tryActivateSkillSlot(entt::entity entity, entt::registry& registry, SkillSlot slot);

    // Events
    EVENT_LISTENER_FUNCS(SkillsComponentSystem, Activate, SkillEventType::Activate, SkillEvent);
    EVENT_LISTENER_FUNCS(SkillsComponentSystem, Interrupt, SkillEventType::Interrupt, SkillEvent);
    EVENT_LISTENER_FUNCS(SkillsComponentSystem, End, SkillEventType::End, SkillEvent);

protected:
    void handleSkillTrigger(World& world, entt::entity entity, SkillsComponent& skillsCmp, ActiveSkillComponent& activeCmp, const SkillTrigger& trigger);
    void handleAttackTrigger(World& world, entt::entity entity, ActiveSkillComponent& activeCmp, const SkillAttackTrigger& attackTrigger);

    EVENT_DISPATCHER_DEF(SkillsComponentSystem);
};