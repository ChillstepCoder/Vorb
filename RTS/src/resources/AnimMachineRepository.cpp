#include "stdafx.h"
#include "AnimMachineRepository.h"

#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"
#include "resources/Blendspace1DRepository.h"

AssetLoadFunc AnimMachineRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        AnimMachineDef& def = *static_cast<AnimMachineDef*>(assetDataPtr);

        YmlSerializer::readFileData(readFileToString(filePath), def);
        if (!def.rigDef.isValid()) {
            panic("AnimMachine {} has no rig", filePath.getString());
        }

        def.addDependency(def.rigDef.getAssetHandle<RigDef>());

        // Validate states and add state dependencies
        for (AnimStateDef& state : def.stateDefs) {
            switch (state.stateType) {
                case AnimStateType::AnimSequence:
                    def.addDependency(AnimationRepository::get().getAssetHandle(state.assetName));
                    break;
                case AnimStateType::Blendspace1D:
                    def.addDependency(Blendspace1DRepository::get().getAssetHandle(state.assetName));
                    break;
                case AnimStateType::Blendspace2D:
                    panic("BlendSpace 2d not implemented yet");
                default:
                    panic("Missing state type on animmachine {} state {}", filePath.getString(), state.name.toString());
            }
            for (auto& transition : state.transitions) {
                if (transition.transitionAnim.isValid()) {
                    def.addDependency(AnimationRepository::get().getAssetHandle(transition.transitionAnim));
                }
            }
        }
        assetLoader.requestAssetLoadWithDependencies([]ASSET_LOAD_LAMBDA(AssetID, filePath, assetDataPtr) {
            AnimMachineDef& def = *static_cast<AnimMachineDef*>(assetDataPtr);
            // Build efficient state representation
            def.states.resize(def.stateDefs.size());
            for (size_t i = 0; i < def.stateDefs.size(); ++i) {
                AnimStateDef& stateDef = def.stateDefs[i];
                AnimState& effState = def.states[i];
                effState.stateType = stateDef.stateType;
                switch (stateDef.stateType) {
                    case AnimStateType::AnimSequence:
                        effState.assetId = AnimationRepository::get().getAssetID(stateDef.assetName);
                        break;
                    case AnimStateType::Blendspace1D:
                        effState.assetId = Blendspace1DRepository::get().getAssetID(stateDef.assetName);
                        break;
                    case AnimStateType::Blendspace2D:
                        panic("BlendSpace 2d not implemented yet");
                    default:
                        panic("Type error post dependency load on anim machine {}", filePath.getString());
                }
                if (stateDef.transitions.size()) {
                    effState.numTransitions = stateDef.transitions.size();
                    effState.transitions = std::make_unique<AnimTransition[]>(effState.numTransitions);
                    for (size_t t = 0; t < stateDef.transitions.size(); ++t) {
                        AnimTransitionDef& transDef = stateDef.transitions[t];
                        AnimTransition& effTrans = effState.transitions[t];
                        if (transDef.transitionAnim.isValid()) {
                            effTrans.transitionAnimID = AnimationRepository::get().getAssetID(transDef.transitionAnim);
                        }
                        effTrans.transitionDuration = transDef.transitionDuration;
                        // Condition
                        if (transDef.condition.isValid()) {
                            const AnimTransitionConditionDef& condDef = getAnimTransitionConditionDef(transDef.condition.defType);
                            effTrans.condition.func = condDef.func;

#define UPDATE_CONDITION_PARAM(type) \
if (std::holds_alternative<f32>(condDef.defaultParam)) { \
    if (std::holds_alternative<type>(transDef.condition.param)) { \
        effTrans.condition.constant = std::get<type>(transDef.condition.param); \
    } \
    else { \
        effTrans.condition.constant = condDef.defaultParam; \
    } \
}
                            UPDATE_CONDITION_PARAM(f32)
                            else UPDATE_CONDITION_PARAM(f32v2)


                            static_assert(std::variant_size_v<AnimParamVar> == 2, "Update construction");
                        }

                        // To state
                        assert(transDef.toState.isValid());
                        for (size_t j = 0; j < def.stateDefs.size(); ++j) {
                            if (def.stateDefs[j].name == transDef.toState) {
                                effTrans.toState = j;
                                break;
                            }
                        }
                        assert(effTrans.toState != INVALID_ANIM_STATE);
                        assert(effTrans.toState != i && "Circular dependency");
                    }
                }
            }

            return true;
        },
            nullptr,
            assetID,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetID].get(),
            nullptr,
            def.getDependencies()
        );
        return false;
    };
}
