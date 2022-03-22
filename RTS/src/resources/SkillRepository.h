#pragma once

#include "definitions/SkillDef.h"

class AnimationRepository;
DECL_VIO(class IOManager);

class SkillRepository
{
public:
    SkillRepository(vio::IOManager& ioManager);
    ~SkillRepository();

    bool loadSkillFile(const vio::Path& filePath, const AnimationRepository& animRepo);

    const SkillDef& getSkillDef(ui32 skillId) const { return mSkillDefs[skillId]; }
    const SkillDef& getSkillDef(const nString& name) const;
    const SkillDef* tryGetSkillDef(const nString& name) const;

private:

    vio::IOManager& mIoManager;
    std::unordered_map<nString, ui32> mSkillIdLookup;
    std::vector<SkillDef> mSkillDefs;
};

