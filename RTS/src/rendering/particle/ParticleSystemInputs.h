#pragma once

enum class ParticleSystemInputNameUInt : ui8 {
    COUNT
};

enum class ParticleSystemInputNameFloat : ui8 {
    COUNT
};

enum class ParticleSystemInputNameVec2: ui8 {
    COUNT
};

enum class ParticleSystemInputNameVec3 : ui8 {
    ImpactDirection,
    ImpactSurfaceNormal,
    COUNT
};

struct ParticleSystemInputs {
    void setUIntInput(ParticleSystemInputNameUInt name, ui32 value) {
        mUIntInputs[name] = value;
    }
    void setFloatInput(ParticleSystemInputNameFloat name, f32 value) {
        mFloatInputs[name] = value;
    }
    void setVec2Input(ParticleSystemInputNameVec2 name, f32v2 value) {
        mVec2Inputs[name] = value;
    }
    void setVec3Input(ParticleSystemInputNameVec3 name, f32v3 value) {
        mVec3Inputs[name] = value;
    }

    ui32 getUIntInput(ParticleSystemInputNameUInt name, ui32 defaultIfNotFound = 0) const {
        auto it = mUIntInputs.find(name);
        if (it != mUIntInputs.end()) [[likely]] return it->second;
        return defaultIfNotFound;
    }

    f32 getFloatInput(ParticleSystemInputNameFloat name, f32 defaultIfNotFound = 0.0f) const {
        auto it = mFloatInputs.find(name);
        if (it != mFloatInputs.end()) [[likely]] return it->second;
        return defaultIfNotFound;
    }

    f32v2 getVec2Input(ParticleSystemInputNameVec2 name, f32v2 defaultIfNotFound = f32v2(0.0f)) const {
        auto it = mVec2Inputs.find(name);
        if (it != mVec2Inputs.end()) [[likely]] return it->second;
        return defaultIfNotFound;
    }

    f32v3 getVec3Input(ParticleSystemInputNameVec3 name, f32v3 defaultIfNotFound = f32v3(0.0f)) const {
        auto it = mVec3Inputs.find(name);
        if (it != mVec3Inputs.end()) [[likely]] return it->second;
        return defaultIfNotFound;
    }

    // TODO: Profile against plain ole vectors for small N
    UnorderedFlatMap<ParticleSystemInputNameUInt, ui32> mUIntInputs;
    UnorderedFlatMap<ParticleSystemInputNameFloat, f32> mFloatInputs;
    UnorderedFlatMap<ParticleSystemInputNameVec2, f32v2> mVec2Inputs;
    UnorderedFlatMap<ParticleSystemInputNameVec3, f32v3> mVec3Inputs;

    f32v3 mInputImpactDirection = f32v3(0.0f, 0.0f, 1.0f);
    f32v3 mInputImpactSurfaceNormal = f32v3(0.0f, 1.0f, 0.0f);

};

using ParticleSystemInputsPtr = std::shared_ptr<ParticleSystemInputs>;