#include "stdafx.h"
#include "CPUParticleEmitterOperation.h"

#include "CpuParticleEmitter.h"

inline void CPUParticleEmitterVariable::evaluate(CpuParticleEmitter& emitter, ParticleID id) {
    // Constants do not evaluate
    if (mType > CPUParticleEmitterVariableType::Constant) {
        if (mType == CPUParticleEmitterVariableType::Operation) {
            mOperation->execute(emitter, id, this);
        }
        else {
            emitter.fillVariableFromType(*this, id, mType);
        }
    }
}
