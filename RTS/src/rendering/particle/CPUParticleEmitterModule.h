#pragma once

#include "ParticleComponentType.h"
#include "CPUParticleEmitterOperation.h"

class ArbitraryObjectArray;

typedef void(*CPUParticleEmitterModuleMethod)(class CpuParticleEmitter& emitter, int particleID, void* data);


class CPUParticleEmitterModule
{
public:

    CPUParticleEmitterModule() { refresh(); }
    virtual ~CPUParticleEmitterModule() = default;

    virtual void addModuleDataToArray(ArbitraryObjectArray& arry) = 0;
    virtual void refresh() = 0;

    bool areAllRequiredComponentsPresent(BitFlags<ParticleComponentType> componentsToCheck) {
        return (mRequiredComponents.getBits() & componentsToCheck.getBits()) == mRequiredComponents.getBits();
    }

    CPUParticleEmitterModuleMethod getMethod() const { return mMethod; }

protected:
    CPUParticleEmitterModuleMethod mMethod;
    BitFlags<ParticleComponentType> mRequiredComponents;

    // TODO: Based on https://docs.unrealengine.com/5.1/en-US/script-editor-reference-for-niagara-effects-in-unreal-engine/
    // TODO: Provided dependencies
    // TODO: Required dependencies
};

