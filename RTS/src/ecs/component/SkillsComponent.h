#pragma once

#include "definitions/SkillDef.h"

struct SkillsComponentFileData {
    Array<nString> mSkillNames;
};
KEG_TYPE_DECL(SkillsComponentFileData);

struct SkillsComponent {
    // TODO: Not vector
    std::vector<const SkillDef*> mSkills;
};
//static_assert(sizeof(SkillsComponent) == 32, "Shrink this later");

