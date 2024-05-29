#include "stdafx.h"
#include "SkillRepository.h"

#include "resources/AnimationRepository.h"
#include "resources/EffectRepository.h"

AssetLoadFunc SkillRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        SkillDef& def = *static_cast<SkillDef*>(assetDataPtr);

        SkillDefFileData fileData;

        YmlSerializer::readFileData(readFileToString(filePath), fileData);

        if (!fileData.mAnimName.isValid()) {
            def.mFlags.setBit(SkillDefFlags::INSTANT);
        }
        else {
            def.mAnimation = fileData.mAnimName;
            def.addDependency(def.mAnimation.getAssetHandleBase());
        }

        // Hit effects
        if (fileData.mHitEffectName.isValid()) {
            def.mHitEffect = fileData.mHitEffectName;
            def.addDependency(def.mHitEffect.getAssetHandleBase());
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
            // Degrees
            newTrigger.mAttackTrigger.mAngle = DEG_TO_RAD(newTrigger.mAttackTrigger.mAngle);
            triggers.emplace_back(std::move(newTrigger));
        }
        std::sort(triggers.begin(), triggers.end(), [](const SkillTrigger& lhs, const SkillTrigger& rhs) -> bool {
            return lhs.mTime < rhs.mTime;
        });

        // Copy sorted triggers
        assert(triggers.size() == def.mNumTriggers);
        memcpy(def.mTriggers.get(), triggers.data(), sizeof(SkillTrigger) * triggers.size());

        // Wait for dependencies to load if needed
        if (def.getDependencies()->getCount()) {
            assetLoader.requestAssetLoadWithDependencies(
                nullptr,
                nullptr,
                assetID,
                assetDataPtr,
                filePath,
                mLoadedAssets[assetID].get(),
                nullptr,
                def.getDependencies()
            );
            return false;
        }
        else {
            return true;
        }
    };
}
