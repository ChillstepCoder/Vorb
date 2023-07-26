#pragma once

typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation;

enum class CPUParticleEmitterVariableType : ui8 {
    Constant,
    Operation,
    BUILTINS_BEGIN,
    Position = BUILTINS_BEGIN, // Builtins beyond here
    Velocity,
    Scale,
    Color,
    HDRColor,
    Lifespan,
    Rotation,
    Custom,
    COUNT
};

class CPUParticleEmitterVariable {
public:
    void evaluate(CpuParticleEmitter& emitter, ParticleID id);

    CPUParticleEmitterOperation* mOperation = nullptr;
    std::variant<color4, f32v4, f32v3, f32v2, f32> mVarData;
    CPUParticleEmitterVariableType mType = CPUParticleEmitterVariableType::Constant;
};

class CPUParticleEmitterOperation {
public:
    CPUParticleEmitterOperation(CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1) : mParam0(p0), mParam1(p1) {}

    CPUParticleEmitterVariable* mParam0 = nullptr;
    CPUParticleEmitterVariable* mParam1 = nullptr;

    virtual void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) = 0;
protected:
    bool evaluateParams(CpuParticleEmitter& emitter, ParticleID id) {
        if (!mParam0 || !mParam1) {
            return false;
        }
        mParam0->evaluate(emitter, id);
        mParam1->evaluate(emitter, id);
        return true;
    }

};

// TODO: AddVec3, addVec3ToFloat, ect
class CPUPEO_AddVec3 : CPUParticleEmitterOperation {
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {
        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32v3>(mParam0->mVarData) + std::get<f32v3>(mParam1->mVarData);
    }
};

class CPUPEO_AddFloatToVec3 : CPUParticleEmitterOperation {
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {
        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32>(mParam0->mVarData) + std::get<f32v3>(mParam1->mVarData);
    }
};
