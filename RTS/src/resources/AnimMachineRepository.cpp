#include "stdafx.h"
#include "AnimMachineRepository.h"

#include "resources/RigRepository.h"

#include <Vorb/io/IOManager.h>

AnimMachineRepository::AnimMachineRepository(vio::IOManager& ioManager, const RigRepository& rigRepository) : mIoManager(ioManager), mRigRepository(rigRepository) {

}

AnimMachineRepository::~AnimMachineRepository() {

}

bool AnimMachineRepository::loadMachineFile(const vio::Path& filePath) {
    AnimMachineDef& def = mAnimMachineDefs.emplace_back();
    def.mAnimMachineId = mAnimMachineDefs.size() - 1u;

    AnimMachineDefFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(AnimMachineDefFileData))) {
        pError("Failed to load anim machine file: " + filePath.getString());
        return false;
    }

    if (fileData.mRigName.empty()) {
        pError("Anim machine file does not have a rig: " + filePath.getString());
        return false;
    }

    const RigDef* rig = mRigRepository.tryGetRigDef(fileData.mRigName);
    if (!rig) {
        pError("Anim machine file rig does not exist: " + filePath.getString());
        return false;
    }

    // Hook up all ozz animation references in the animation machine
    const nString* animIter = &fileData.mWalkLeftName; //< Must be mWalkLeftName as it is start of the string array
    for (ui32 i = 0; i < ANIMATION_MACHINE_ANIMS_COUNT; ++i) {
        if (animIter->size()) {
            // Search for corresponding animation in the rigdef
            auto&& it = rig->mNameToAnimationIndex.find(*animIter);
            if (it != rig->mNameToAnimationIndex.end()) {
                def.mAnimsArray[i] = &rig->mAnimations[it->second];
            } else {
                pError("Anim machine animation " + *animIter + " does not exist in rig. Machine file: " + filePath.getString());
                return false;
            }
        }
        ++animIter;
    }

    mAnimMachineIdLookup[filePath.getFileNameNoExtension()] = def.mAnimMachineId;
    return true;
}

const AnimMachineDef& AnimMachineRepository::getAnimMachineDef(const nString& name) const {
    auto&& it = mAnimMachineIdLookup.find(name);
    assert(it != mAnimMachineIdLookup.end());
    return mAnimMachineDefs[it->second];
}

const AnimMachineDef* AnimMachineRepository::tryGetAnimMachineDef(const nString& name) const {
    auto&& it = mAnimMachineIdLookup.find(name);
    if (it == mAnimMachineIdLookup.end()) {
        return nullptr;
    }
    return &mAnimMachineDefs[it->second];
}
