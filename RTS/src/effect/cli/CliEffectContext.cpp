#include "stdafx.h"
#include "CliEffectContext.h"

#include "resources/EffectRepository.h"
#include "rendering/particle/CPUParticleSystem.h"
#include "rendering/particle/ParticleSystemRenderer.h"

#include "resources/ModelRepository.h"

#include "camera/Camera3D.h"

#include "util/MathUtil.hpp"

EffectInstance::EffectInstance(const EffectDef* effectDef, const ParticleSystemDef* systemDef, f32v3 position, f32q orientation, ParticleSystemInputsPtr inputs) : mEffectDef(effectDef) {
    assert(systemDef); // TODO: Allow effects with no particle system?
    mSystem = std::make_unique<CPUParticleSystem>(*systemDef, position, orientation, std::move(inputs));
}
EffectInstance::~EffectInstance() = default;

void CliEffectContext::renderEffects(f32 elapsedSec, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();

    PROFILE_FUNCTION();
    // Pending asset load effects
    for (auto&& it = mPendingAssetLoadEffects.begin(); it != mPendingAssetLoadEffects.end();) {
        PendingEffectData& data = it->second;
        if (const EffectDef* def = data.mEffectHandle->tryGetLoadedAsset()) {
            mEffectInstances.reserve(mEffectInstances.size() + data.mPendingInstances.size());
            const ParticleSystemDef* sysDef = def->getLoadedParticleSystemDef();
            for (PendingEffectInstanceData& instance : data.mPendingInstances) {
                addEffectInstance(
                    data.mEffectHandle->getAssetID(),
                    EffectInstance(def, sysDef, instance.position, instance.orientation, instance.inputs)
                );
            }
            it = mPendingAssetLoadEffects.erase(it);
        }
        else {
            ++it;
        }
    }

    // Render queue
    constexpr size_t BULK_SIZE = 64;
    std::pair<EffectAssetRef, PendingEffectInstanceData> effects[BULK_SIZE];
    if (size_t count = mRenderThreadQueue.try_dequeue_bulk(effects, BULK_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            auto&& data = effects[i];
            playParticleEffectAtPoint(data.first, data.second.position, data.second.orientation, data.second.inputs, data.second.flags);
        }
    }

    // TODO: Frustum Culling
    // Render each effect and optionally remove them on death
    {
        PROFILE_SCOPE("Update Effects");
        for (auto&& it = mEffectInstances.begin(); it != mEffectInstances.end();) {
            if (it->mSystem->update(elapsedSec, mEmitterRenderList)) {
                // Decref
                auto&& eit = mEffectReferences.find(it->mEffectDef);
                assert(eit != mEffectReferences.end());
                if (--eit->second.first == 0) {
                    mEffectReferences.erase(eit);
                }
                // Linear erase but usually not a ton of VFX getting killed each frame
                // and these are small. We want to preserve sort order so we get no visible
                // popping.
                it = mEffectInstances.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    ParticleSystemRenderer::renderEmitters(mEmitterRenderList, camera);
}

void CliEffectContext::playParticleEffectAtPoint(EffectAssetRef effectName, f32v3 point, f32q orientation, ParticleSystemInputsPtr inputs, BitFlags<EffectCreateFlags> flags) {
    if (IS_RENDER_THREAD()) {
        AssetHandlePtr<EffectDef> effectHandle = effectName.getAssetHandle<EffectDef>();
        if (const EffectDef* effectDef = effectHandle->tryGetLoadedAsset()) {
            addEffectInstance(
                effectHandle->getAssetID(),
                EffectInstance(effectDef, effectDef->getLoadedParticleSystemDef(), point, orientation, std::move(inputs))
            );
        }
        else {
            auto it = mPendingAssetLoadEffects.find(effectName);
            if (it != mPendingAssetLoadEffects.end()) {
                it->second.mPendingInstances.emplace_back(point, orientation, std::move(inputs), flags);
            }
            else {
                auto newIt = mPendingAssetLoadEffects.insert(std::make_pair(effectName, PendingEffectData{
                    .mEffectHandle = std::move(effectHandle)
                    })
                ).first;
                newIt->second.mPendingInstances.emplace_back(point, orientation, std::move(inputs), flags);
            }
        }
    } else {
        mRenderThreadQueue.enqueue(std::make_pair(effectName, PendingEffectInstanceData(point, orientation, std::move(inputs), flags)));
    }
}

void CliEffectContext::playMutationEffect(const f32m4& transform, ModelID startModel, ModelID endModel, TileMutationType mutationType, BitFlags<EffectCreateFlags> flags) {

    ParticleSystemInputsPtr inputs = std::make_unique<ParticleSystemInputs>();
    f32v3 scale;
    f32q rot;
    f32v3 translation;
    MathUtil::decomposeMatrix(transform, scale, rot, translation);

    inputs->setFloatInput(ParticleSystemInputName::FloatSourceScale, scale.x);
    inputs->setFloatInput(ParticleSystemInputName::FloatTargetScale, scale.x);
    inputs->setMeshInput(ParticleSystemInputName::MeshSource, ParticleSystemMeshInput{ &ModelRepository::get().getLoadedOrUnloadedAsset(startModel) });
    inputs->setMeshInput(ParticleSystemInputName::MeshTarget, ParticleSystemMeshInput{ &ModelRepository::get().getLoadedOrUnloadedAsset(endModel) });

    EffectAssetRef effectName;

    switch (mutationType) {
        case TileMutationType::BCorrupt:
            assert(false);
            break;
        case TileMutationType::BPurify:
            assert(false);
            break;
        case TileMutationType::CCorrupt:
            effectName = CStrToken("c_corrupt");
            break;
        case TileMutationType::CPurify:
            assert(false);
            break;
        case TileMutationType::Grow:
            assert(false);
            break;
        case TileMutationType::Decay:
            assert(false);
            break;
        case TileMutationType::COUNT:
            break;
        default:
            assert(false);

    }
    static_assert(e_count(TileMutationType) == 6, "Please update this switch statement");

    playParticleEffectAtPoint(effectName, translation, rot, std::move(inputs), flags);
}

void CliEffectContext::addEffectInstance(AssetID assetId, EffectInstance instance) {
    // Refcounting
    auto&& eit = mEffectReferences.find(instance.mEffectDef);
    if (eit != mEffectReferences.end()) {
        // If it is in mEffectReferences, it is loaded. Incref
        ++eit->second.first;
    }
    else {
        mEffectReferences.insert(
            std::make_pair(
                instance.mEffectDef,
                std::make_pair(1, EffectRepository::get().getAssetHandle(assetId))
            )
        );
    }
    mEffectInstances.emplace_back(std::move(instance));
}
