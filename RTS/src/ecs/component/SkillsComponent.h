#pragma once

#include <Vorb/Event.hpp>
#include "definitions/SkillDef.h"

#include "events/SkillEvent.h"

class World;

struct SkillsComponentFileData {
    Array<nString> mSkillNames;
};
KEG_TYPE_DECL(SkillsComponentFileData);

struct ActiveSkillComponent {
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
    void handleAttackTrigger(World& world, entt::entity entity, const SkillAttackTrigger& attackTrigger);

    EVENT_DISPATCHER_DEF(SkillsComponentSystem);
};