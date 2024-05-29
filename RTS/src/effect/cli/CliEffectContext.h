#pragma once

#include "effect/IEffectContext.h"
#include "definitions/EffectDef.h"

#include <concurrent_queue.h>

class CPUParticleSystem;

class PendingEffectInstanceData {
public:
    PendingEffectInstanceData() = default;
    PendingEffectInstanceData(f32v3 position, ParticleSystemInputs inputs, BitFlags<EffectCreateFlags> flags) :
        position(position),
        inputs(inputs),
        flags(flags) {}

    f32v3 position;
    ParticleSystemInputs inputs;
    BitFlags<EffectCreateFlags> flags;
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
        EffectAssetRef effectName,
        f32v3 point,
        ParticleSystemInputs inputs,
        BitFlags<EffectCreateFlags> flags
    ) override;

private:
    void addEffectInstance(AssetID assetId, EffectInstance instance);

    moodycamel::ConcurrentQueue<std::pair<EffectAssetRef, PendingEffectInstanceData>> mRenderThreadQueue;

    std::unordered_map<EffectAssetRef, PendingEffectData> mPendingAssetLoadEffects;
    std::vector<EffectInstance> mEffectInstances;
    std::unordered_map<const EffectDef*, std::pair<int, AssetHandlePtr<EffectDef>>> mEffectReferences;
};

