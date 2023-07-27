#pragma once

class CPUParticleEmitterOperation;
class CpuParticleEmitter;

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

typedef std::variant<color4, f32v4, f32v3, f32v2, f32> CPUParticleEmitterVariantData;

class CPUParticleEmitterVariable {
public:
    CPUParticleEmitterVariable() = default;
    CPUParticleEmitterVariable(CPUParticleEmitterVariantData data) : mVarData(data) {}

    void evaluate(CpuParticleEmitter& emitter, ParticleID id);

    CPUParticleEmitterOperation* mOperation = nullptr;
    CPUParticleEmitterVariantData mVarData;
    CPUParticleEmitterVariableType mType = CPUParticleEmitterVariableType::Constant;
};

// TODO: test perf vs virtual func
//typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation {
public:
    CPUParticleEmitterOperation(CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1) : mParam0(p0), mParam1(p1) {}

    CPUParticleEmitterVariable* mParam0 = nullptr;
    CPUParticleEmitterVariable* mParam1 = nullptr;

    virtual constexpr const char* getDisplayName() const = 0;

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

class CPUPEO_AddVec3 : CPUParticleEmitterOperation {
    constexpr const char* getDisplayName() const override { return "Add Vec3"; }

    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {

        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32v3>(mParam0->mVarData) + std::get<f32v3>(mParam1->mVarData);
    }
};

class CPUPEO_MultiplyVec3 : CPUParticleEmitterOperation {
    constexpr const char* getDisplayName() const override { return "Multiply Vec3"; }

    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {

        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32v3>(mParam0->mVarData) * std::get<f32v3>(mParam1->mVarData);
    }
};

class CPUPEO_AddFloatToVec3 : CPUParticleEmitterOperation {
    constexpr const char* getDisplayName() const override { return "Add Float To Vec3"; }

    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {
        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32>(mParam0->mVarData) + std::get<f32v3>(mParam1->mVarData);
    }
};

class CPUPEO_MultiplyFloatToVec3 : CPUParticleEmitterOperation {
    constexpr const char* getDisplayName() const override { return "Multiply Float To Vec3"; }

    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override {
        if (!evaluateParams(emitter, id)) return;
        output->mVarData = std::get<f32>(mParam0->mVarData) * std::get<f32v3>(mParam1->mVarData);
    }
};
