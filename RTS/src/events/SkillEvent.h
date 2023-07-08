#pragma once

#include "skill/SkillSlot.h"

#include <Vorb/Event.hpp>

enum class SkillEventType {
    Activate,
    Interrupt,
    End
};

struct SkillEvent {
    entt::entity mEntity;
    SkillSlot mSkillSlot;
};
EVENT_DISPATCHER_TYPE(SkillsComponentSystem, SkillEventType, SkillEvent);