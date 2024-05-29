#include "stdafx.h"
#include "CliEffectContext.h"

#include "resources/EffectRepository.h"
#include "rendering/particle/CPUParticleSystem.h"

#include "camera/Camera3D.h"

EffectInstance::EffectInstance(const EffectDef* effectDef, const ParticleSystemDef* systemDef, f32v3 position, const ParticleSystemInputs& inputs) : mEffectDef(effectDef) {
    assert(systemDef); // TODO: Allow effects with no particle system?
    mSystem = std::make_unique<CPUParticleSystem>(*systemDef, position);
    mSystem->setInputs(inputs);
}
EffectInstance::~EffectInstance() = default;

void CliEffectContext::renderEffects(f32 elapsedSec, const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    glDisable(GL_CULL_FACE);

    // Pending asset load effects
    for (auto&& it = mPendingAssetLoadEffects.begin(); it != mPendingAssetLoadEffects.end();) {
        PendingEffectData& data = it->second;
        if (const EffectDef* def = data.mEffectHandle->tryGetLoadedAsset()) {
            mEffectInstances.reserve(mEffectInstances.size() + data.mPendingInstances.size());
            const ParticleSystemDef* sysDef = def->getLoadedParticleSystemDef();
            for (PendingEffectInstanceData& instance : data.mPendingInstances) {
                addEffectInstance(
                    data.mEffectHandle->getAssetID(),
                    EffectInstance(def, sysDef, instance.position, instance.inputs)
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
            playParticleEffectAtPoint(data.first, data.second.position, data.second.inputs, data.second.flags);
        }
    }

    // TODO: Frustum Culling
    // Render each effect and optionally remove them on death
    for (auto&& it = mEffectInstances.begin(); it != mEffectInstances.end();) {
        if (it->mSystem->updateAndRender(elapsedSec, camera.getVPMatrix())) {
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

void CliEffectContext::playParticleEffectAtPoint(EffectAssetRef effectName, f32v3 point, ParticleSystemInputs inputs, BitFlags<EffectCreateFlags> flags) {
    if (IS_RENDER_THREAD()) {
        AssetHandlePtr<EffectDef> effectHandle = effectName.getAssetHandle<EffectDef>();
        if (const EffectDef* effectDef = effectHandle->tryGetLoadedAsset()) {
            addEffectInstance(
                effectHandle->getAssetID(),
                EffectInstance(effectDef, effectDef->getLoadedParticleSystemDef(), point, inputs)
            );
        }
        else {
            auto&& it = mPendingAssetLoadEffects.find(effectName);
            if (it != mPendingAssetLoadEffects.end()) {
                it->second.mPendingInstances.emplace_back(PendingEffectInstanceData(point, inputs, flags));
            }
            else {
                mPendingAssetLoadEffects.insert(std::make_pair(effectName, PendingEffectData{
                    .mEffectHandle = std::move(effectHandle),
                    .mPendingInstances =
                        std::vector<PendingEffectInstanceData>{PendingEffectInstanceData(point, inputs, flags)}
                    })
                );
            }
        }
    } else {
        mRenderThreadQueue.enqueue(std::make_pair(effectName, PendingEffectInstanceData(point, inputs, flags)));
    }
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
