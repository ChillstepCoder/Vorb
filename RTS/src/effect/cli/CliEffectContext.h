#pragma once

#include "effect/IEffectContext.h"
#include "definitions/EffectDef.h"

class CPUParticleSystem;

struct PendingEffectInstanceData {
    f32v3 position;
    ParticleSystemInputs inputs;
};

class EffectInstance {
public:
    EffectInstance(const EffectDef* effectDef, const ParticleSystemDef* systemDef, f32v3 position, const ParticleSystemInputs& inputs);
    ~EffectInstance();

    VORB_NON_COPYABLE_BUT_MOVABLE(EffectInstance);

    const EffectDef* mEffectDef = nullptr;
    std::unique_ptr<CPUParticleSystem> mSystem;
};

struct PendingEffectData {
    AssetHandlePtr<EffectDef> mEffectHandle;
    std::vector<PendingEffectInstanceData> mPendingInstances;
};

class CliEffectContext : public IEffectContext {
    friend class HostEffectContext;
public:
    CliEffectContext(World& world) : IEffectContext(world) {}

    void renderEffects(f32 elapsedSec, const Camera3D& camera) override;

    void playParticleEffectAtPoint(
        StrToken effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;

private:
    void addEffectInstance(AssetHandlePtr<EffectDef>&& assetHandle, EffectInstance instance);

    std::unordered_map<StrToken, PendingEffectData> mPendingEffects;
    std::vector<EffectInstance> mEffectInstances;
    std::unordered_map<const EffectDef*, std::pair<int, AssetHandlePtr<EffectDef>>> mEffectReferences;
};

