#pragma once

#include "definitions/SkillDef.h"

struct SkillsComponentFileData {
    Array<nString> mSkillNames;
};
KEG_TYPE_DECL(SkillsComponentFileData);

struct SkillsComponent {
    std::vector<const SkillDef*> mSkills;
};

