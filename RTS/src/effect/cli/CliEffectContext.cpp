#include "stdafx.h"
#include "CliEffectContext.h"

#include "resources/EffectRepository.h"
#include "rendering/particle/CPUParticleSystem.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include "camera/Camera3D.h"

EffectInstance::EffectInstance(const EffectDef* effectDef, const ParticleSystemDef* systemDef, f32v3 position, ParticleSystemInputsPtr inputs) : mEffectDef(effectDef) {
    assert(systemDef); // TODO: Allow effects with no particle system?
    mSystem = std::make_unique<CPUParticleSystem>(*systemDef, position, std::move(inputs));
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
    {
        PROFILE_SCOPE("Render Effects");
        glDisable(GL_CULL_FACE);
        for (int blendMode = 0; blendMode < e_count(ParticleBlendMode); ++blendMode) {
            FlatMap<AssetID /*materialShader*/, std::vector<EmitterRenderData>>& emitters = mEmitterRenderList[blendMode];
            if (emitters.empty()) {
                continue;
            }
            bindStateForParticleBlendMode((ParticleBlendMode)blendMode);

            static_assert(e_count(ParticleBlendMode) == 4, "Update switch statement for new ParticleBlendMode enum values");
            for (auto& [shaderID, emitterList] : emitters) {
                const MaterialShaderDef* shader = MaterialShaderRepository::get().tryGetLoadedAsset(shaderID);
                if (!shader) {
                    continue;
                }
                VGUniform unRootPos = shader->getUniform("unRootPos");
                MaterialRenderer::bindMaterialShaderForRender(*shader);
                glUniformMatrix4fv(shader->getUniform("unVP"), 1, false, &camera.getVPMatrix()[0][0]);
                for (auto& renderData : emitterList) {
                    glUniform3fv(unRootPos, 1, (const GLfloat*)renderData.rootPosition);
                    renderData.emitter->render();
                }
            }

            emitters.clear();
        }
    }
}

void CliEffectContext::playParticleEffectAtPoint(EffectAssetRef effectName, f32v3 point, ParticleSystemInputsPtr inputs, BitFlags<EffectCreateFlags> flags) {
    if (IS_RENDER_THREAD()) {
        AssetHandlePtr<EffectDef> effectHandle = effectName.getAssetHandle<EffectDef>();
        if (const EffectDef* effectDef = effectHandle->tryGetLoadedAsset()) {
            addEffectInstance(
                effectHandle->getAssetID(),
                EffectInstance(effectDef, effectDef->getLoadedParticleSystemDef(), point, std::move(inputs))
            );
        }
        else {
            auto it = mPendingAssetLoadEffects.find(effectName);
            if (it != mPendingAssetLoadEffects.end()) {
                it->second.mPendingInstances.emplace_back(point, std::move(inputs), flags);
            }
            else {
                auto newIt = mPendingAssetLoadEffects.insert(std::make_pair(effectName, PendingEffectData{
                    .mEffectHandle = std::move(effectHandle)
                    })
                ).first;
                newIt->second.mPendingInstances.emplace_back(point, std::move(inputs), flags);
            }
        }
    } else {
        mRenderThreadQueue.enqueue(std::make_pair(effectName, PendingEffectInstanceData(point, std::move(inputs), flags)));
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
