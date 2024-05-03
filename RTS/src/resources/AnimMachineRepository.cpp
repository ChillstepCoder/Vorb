#include "stdafx.h"
#include "AnimMachineRepository.h"

#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"
#include "resources/Blendspace1DRepository.h"

#include "rendering/animation/AnimMachineInstance.h"

void AnimMachineRepository::fixupLoadedAsset(AssetID assetId) {
    AnimMachineDef& def = getMutableAssetInternal(assetId);
    
    // Build efficient state representation
    def.states.clear(); // Clear old data so we can rebuild it
    def.totalBlendspace1Ds = 0;
    def.totalBlendspace2Ds = 0;
    def.states.resize(def.stateDefs.size());
    for (size_t i = 0; i < def.stateDefs.size(); ++i) {
        AnimMachineStateDef& stateDef = def.stateDefs[i];
        AnimMachineState& effState = def.states[i];
        effState.stateType = stateDef.stateType;
        switch (stateDef.stateType) {
            case AnimStateType::AnimSequence:
                effState.assetId = stateDef.assetRef.getAssetID();
                break;
            case AnimStateType::Blendspace1D:
                effState.assetId = stateDef.assetRef.getAssetID();
                ++def.totalBlendspace1Ds;
                break;
            case AnimStateType::Blendspace2D:
                panic("BlendSpace 2d not implemented yet");
                ++def.totalBlendspace2Ds;
            default:
                panic("Type error post dependency load on anim machine {}", def.getName().toString());
        }
        if (stateDef.transitions.size()) {
            effState.numTransitions = stateDef.transitions.size();
            effState.transitions = std::make_unique<AnimTransition[]>(effState.numTransitions);
            for (size_t t = 0; t < stateDef.transitions.size(); ++t) {
                AnimTransitionDef& transDef = stateDef.transitions[t];
                AnimTransition& effTrans = effState.transitions[t];
                if (transDef.transitionAnim.isValid()) {
                    effTrans.transitionAnimID = transDef.transitionAnim.getAssetID();
                }
                effTrans.transitionDuration = transDef.transitionDuration;
                // Condition
                if (transDef.condition.isValid()) {
                    const AnimTransitionConditionDef& condDef = getAnimTransitionConditionDef(transDef.condition.defType);
                    effTrans.condition.func = condDef.func;

#define UPDATE_CONDITION_PARAM(type) \
if (std::holds_alternative<type>(condDef.defaultParam)) { \
    if (std::holds_alternative<type>(transDef.condition.param)) { \
        effTrans.condition.constant = std::get<type>(transDef.condition.param); \
    } \
    else { \
        effTrans.condition.constant = std::get<type>(condDef.defaultParam); \
    } \
}
                    UPDATE_CONDITION_PARAM(f32)
                    else UPDATE_CONDITION_PARAM(f32v2)

                    static_assert(std::variant_size_v<AnimParamVar> == 3, "Update construction");
                }

                // To state
                if (transDef.toState.isValid()) {
                    for (size_t j = 0; j < def.stateDefs.size(); ++j) {
                        if (def.stateDefs[j].name == transDef.toState) {
                            effTrans.toState = j;
                            break;
                        }
                    }
                }
                else {
                    LOG_WARN("Invalid ToState on {}", stateDef.name.toString());
                }
                if (effTrans.toState == INVALID_ANIM_STATE) {
                    transDef.toState.clear();
                }
                assert(effTrans.toState != i && "Circular dependency");
            }
        }
    }

    buildInstanceTemplate(def);
}

void AnimMachineRepository::buildInstanceTemplate(AnimMachineDef& def) {
    // Heavy lifting done in this function so future instantiation is simply a copy
    def.instanceTemplate = std::make_unique<AnimMachineInstance>();
    AnimMachineInstance& inst = *def.instanceTemplate;
    // Default instance does not have a machine def handle
    assert(def.totalBlendspace1Ds < UINT8_MAX);
    inst.rigDef = &def.rigDef.getAssetHandle<RigDef>()->getLoadedAsset();
    inst.numStates = def.states.size();
    inst.numBlendspace1DPlayers = def.totalBlendspace1Ds;
    inst.states = std::make_unique<AnimMachineInstanceState[]>(inst.numStates);
    inst.blendspace1DPlayers = std::make_unique<Blendspace1DPlayer[]>(inst.numBlendspace1DPlayers);
    int blendspace1DIndex = 0;
    for (size_t stateIndex = 0; stateIndex < def.states.size(); ++stateIndex) {
        AnimMachineInstanceState& instState = inst.states[stateIndex];
        const AnimMachineState& defState = def.states[stateIndex];
        instState.stateType = defState.stateType;
        switch (instState.stateType) {
            case AnimStateType::AnimSequence:
                instState.anim.animDef = defState.assetId;
                instState.anim.time = 0.0f;
                break;
            case AnimStateType::Blendspace1D:
                assert(blendspace1DIndex < def.totalBlendspace1Ds);
                instState.blendspace1d.playerId = blendspace1DIndex++;
                inst.blendspace1DPlayers[instState.blendspace1d.playerId] = Blendspace1DPlayer(Blendspace1DRepository::get().getLoadedAsset(defState.assetId));
                break;
            case AnimStateType::Blendspace2D:
                break;
            default:
                panic("Invalid state type");
                break;

        }
        static_assert(e_count(AnimStateType) == 3);
    }
}

AssetLoadFunc AnimMachineRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        AnimMachineDef& def = *static_cast<AnimMachineDef*>(assetDataPtr);

        YmlSerializer::readFileData(readFileToString(filePath), def);
        if (!def.rigDef.isValid()) {
            panic("AnimMachine {} has no rig", filePath.getString());
        }

        def.addDependency(def.rigDef.getAssetHandle<RigDef>());

        // Validate states and add state dependencies
        for (AnimMachineStateDef& state : def.stateDefs) {
            switch (state.stateType) {
                case AnimStateType::AnimSequence:
                    state.assetRef.assetType = AssetType::Animation;
                    def.addDependency(state.assetRef.getAssetHandleBase());
                    break;
                case AnimStateType::Blendspace1D:
                    state.assetRef.assetType = AssetType::Blendspace1D;
                    def.addDependency(state.assetRef.getAssetHandleBase());
                    break;
                case AnimStateType::Blendspace2D:
                    panic("BlendSpace 2d not implemented yet");
                default:
                    panic("Missing state type on animmachine {} state {}", filePath.getString(), state.name.toString());
            }
            static_assert(e_count(AnimStateType) == 3);
            for (auto& transition : state.transitions) {
                if (transition.transitionAnim.isValid()) {
                    def.addDependency(transition.transitionAnim.getAssetHandleBase());
                }
            }
        }
        assetLoader.requestAssetLoadWithDependencies([this]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
            fixupLoadedAsset(assetID);
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
