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

    // Copy skill triggers
    def.mNumTriggers = fileData.mAttackTriggers.size() + fileData.mSimpleTriggers.size();
    def.mTriggers = std::unique_ptr<SkillTrigger[]>(new SkillTrigger[def.mNumTriggers]);

    // Build sortable triggers array
    std::vector<SkillTrigger> triggers;
    triggers.reserve(fileData.mAttackTriggers.size() + fileData.mSimpleTriggers.size());
    for (ui32 i = 0; i < fileData.mSimpleTriggers.size(); ++i) {
        const SkillSimpleTriggerFileData& simpleData = fileData.mSimpleTriggers[i];
        SkillTrigger newTrigger;
        newTrigger.mTime = simpleData.mTime;
        newTrigger.mType = SkillTriggerType::Simple;
        newTrigger.mSimpleTrigger = simpleData.mId;
        triggers.emplace_back(std::move(newTrigger));
    }
    for (ui32 i = 0; i < fileData.mAttackTriggers.size(); ++i) {
        const SkillAttackTriggerFileData& attackData = fileData.mAttackTriggers[i];
        SkillTrigger newTrigger;
        newTrigger.mTime = attackData.mTime;
        newTrigger.mType = SkillTriggerType::Attack;
        newTrigger.mAttackTrigger = attackData.mData;
        triggers.emplace_back(std::move(newTrigger));
    }
    std::sort(triggers.begin(), triggers.end(), [](const SkillTrigger& lhs, const SkillTrigger& rhs) -> bool {
        return lhs.mTime < rhs.mTime;
    });

    // Copy sorted triggers
    assert(triggers.size() == def.mNumTriggers);
    memcpy(def.mTriggers.get(), triggers.data(), sizeof(SkillTrigger) * triggers.size());

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
