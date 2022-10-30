#include "stdafx.h"
#include "SkillRepository.h"

#include "resources/AnimationRepository.h"

#include <Vorb/io/IOManager.h>

SkillRepository::SkillRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

SkillRepository::~SkillRepository() {

}

bool SkillRepository::loadSkillFile(const vio::Path& filePath, const AnimationRepository& animRepo) {

    SkillDef& def = mSkillDefs.emplace_back();
    def.mSkillId = mSkillDefs.size() - 1u;

    SkillDefFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(SkillDefFileData))) {
        pError("Failed to load skill file " + filePath.getString());
        return false;
    }

    if (fileData.mAnimName.empty()) {
        def.mFlags.setBit(SkillDefFlags::INSTANT);
    }
    else {
        def.mAnimID = animRepo.getAnimationID(fileData.mAnimName);
    }

    def.mDuration = fileData.mDuration;
    def.mCost = fileData.mCost;

    assert(fileData.mAttackTriggers.size() <= MAX_SKILL_ATTACK_TRIGGERS);
    // Copy skill triggers
    def.mNumAttackTriggers = fileData.mAttackTriggers.size();
    for (ui32 i = 0; i < fileData.mAttackTriggers.size(); ++i) {
        def.mAttackTriggers[i] = fileData.mAttackTriggers[i];
    }

    const nString skillName = filePath.getFileNameNoExtension();
    assert(mSkillIdLookup.find(skillName) == mSkillIdLookup.end());
    mSkillIdLookup[skillName] = def.mSkillId;
   
    return true;
}

const SkillDef& SkillRepository::getSkillDef(const nString& name) const {
    auto&& it = mSkillIdLookup.find(name);
    assert(it != mSkillIdLookup.end());
    return mSkillDefs[it->second];
}

const SkillDef* SkillRepository::tryGetSkillDef(const nString& name) const {
    auto&& it = mSkillIdLookup.find(name);
    if (it == mSkillIdLookup.end()) {
        return nullptr;
    }
    return &mSkillDefs[it->second];
}
