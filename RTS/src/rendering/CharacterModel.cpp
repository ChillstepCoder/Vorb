#include "stdafx.h"
#include "CharacterModel.h"

#include "resources/ModelRepository.h"
#include "definitions/RigDef.h"

void CharacterModelComponent::addLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel) {
    const ModelDef& baseModelDef = ModelRepository::get().getLoadedOrUnloadedAsset(model.baseModel);
    assert(baseModelDef.mRig);
    ui8 boneIndex = 0;
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        auto jit = baseModelDef.mRig->mJointNameToIndex.find(linkedSubmodel.attachBone);
        assert(jit != baseModelDef.mRig->mJointNameToIndex.end());
        boneIndex = jit->second;
    }

    linkedSubmodels.emplace_back(LinkedSubmodelData{ linkedSubmodel.modelId, boneIndex });
    CharacterModelEvents::dispatchSubmodelAdded({ thisEntity, linkedSubmodels.back() });
}

void CharacterModelComponent::removeLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel) {
    const ModelDef& baseModelDef = ModelRepository::get().getLoadedOrUnloadedAsset(model.baseModel);
    assert(baseModelDef.mRig);
    ui8 boneIndex = 0;
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        auto jit = baseModelDef.mRig->mJointNameToIndex.find(linkedSubmodel.attachBone);
        assert(jit != baseModelDef.mRig->mJointNameToIndex.end());
        boneIndex = jit->second;
    }

    const LinkedSubmodelData cmp = { linkedSubmodel.modelId, boneIndex };
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        if (linkedSubmodels[i] == cmp) {
            linkedSubmodels[i] = std::move(linkedSubmodels[linkedSubmodels.size() - 1]);
            linkedSubmodels.pop_back();
            CharacterModelEvents::dispatchSubmodelRemoved({ thisEntity, cmp });
            return;
        }
    }
}

void CharacterModelComponent::toggleLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel) {
    const ModelDef& baseModelDef = ModelRepository::get().getLoadedOrUnloadedAsset(model.baseModel);
    assert(baseModelDef.mRig);
    ui8 boneIndex = 0;
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        auto jit = baseModelDef.mRig->mJointNameToIndex.find(linkedSubmodel.attachBone);
        assert(jit != baseModelDef.mRig->mJointNameToIndex.end());
        boneIndex = jit->second;
    }

    const LinkedSubmodelData cmp = { linkedSubmodel.modelId, boneIndex };
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        if (linkedSubmodels[i] == cmp) {
            linkedSubmodels[i] = std::move(linkedSubmodels[linkedSubmodels.size() - 1]);
            linkedSubmodels.pop_back();
            CharacterModelEvents::dispatchSubmodelRemoved({ thisEntity, cmp });
            return;
        }
    }
    linkedSubmodels.emplace_back(cmp);
    CharacterModelEvents::dispatchSubmodelAdded({ thisEntity, cmp });
}

bool CharacterModelComponent::hasLinkedSubmodel(LinkedSubmodelData submodel) {
    return std::find(linkedSubmodels.begin(), linkedSubmodels.end(), submodel) != linkedSubmodels.end();
}

CharacterModelComponent::CharacterModelComponent(ModularHumanoidCharacterModel model, const std::vector<LinkedSubmodelDesc>* inLinkedSubmodels) :
    model(model) {

    const ModelDef& baseModelDef = ModelRepository::get().getLoadedOrUnloadedAsset(model.baseModel);
    assert(baseModelDef.mRig);
    // TODO: Can we eliminate this lookup by doing it earlier?
    if (inLinkedSubmodels) {
        linkedSubmodels.resize(inLinkedSubmodels->size());
        for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
            auto jit = baseModelDef.mRig->mJointNameToIndex.find(inLinkedSubmodels->operator[](i).attachBone);
            assert(jit != baseModelDef.mRig->mJointNameToIndex.end());
            linkedSubmodels[i].modelId = inLinkedSubmodels->operator[](i).modelId;
            linkedSubmodels[i].attachBoneIndex = jit->second;
        }
    }
}
