#pragma once

typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation;

#define OPERATION_DEF(x) \
{

}

enum class CPUParticleEmitterVariableBaseType : ui8 {
    EMPTY,
    f32,
    f32v2,
    f32v3,
    color4
};

enum class CPUParticleEmitterVariableType : ui8 {
    Operation,
    Constant,
    Position, // Builtins beyond here
    Velocity,
    Scale,
    Color,
    HDRColor,
    Lifespan,
    Rotation,
    Custom,
};

struct CPUParticleEmitterVariable {
    CPUParticleEmitterOperation* mOperation;
    union {
        color4 mColor4;
        f32v3 mVec3;
        f32v2 mVec2;
        f32 mF32;
    };
    CPUParticleEmitterVariableBaseType mBaseType = CPUParticleEmitterVariableBaseType::EMPTY;
    CPUParticleEmitterVariableType mType = CPUParticleEmitterVariableType::Constant;

    void evaluate() {
        if (mType > CPUParticleEmitterVariableType::Constant) {
            dddd;
        }
        else if (mType == CPUParticleEmitterVariableType::Operation) {
            mOperation->execute(this);
        }
    }
};

class CPUParticleEmitterOperation {
public:
    CPUParticleEmitterOperation(CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1) : mParam0(p0), mParam1(p1) {}

    CPUParticleEmitterVariable* mParam0 = nullptr;
    CPUParticleEmitterVariable* mParam1 = nullptr;
    //CPUParticleEmitterOperationMethod mMethod = nullptr;

    virtual void execute(CPUParticleEmitterVariable* output) = 0;
};

// TODO: AddVec3, addVec3ToFloat, ect
class CPUPEO_Add : CPUParticleEmitterOperation {
    void execute(CPUParticleEmitterVariable* output) override {
        if (!mParam0 || !mParam1) {
            return;
        }
        mParam0->evaluate();
        mParam1->evaluate();
    }
};
