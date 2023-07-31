#pragma once

#include "ParticleComponentType.h"
#include "CPUParticleEmitterOperation.h"

class ArbitraryObjectArray;

typedef void(*CPUParticleEmitterModuleMethod)(class CpuParticleEmitter& emitter, int particleID, void* data);

enum class ParticleEmitterModuleStage : ui8 {
    EmitterUpdate = BIT(0),
    ParticleInit = BIT(1),
    ParticleUpdate = BIT(2),
};

class CPUParticleEmitterModule
{
public:
    CPUParticleEmitterModule() = default;
    virtual ~CPUParticleEmitterModule() = default;

    virtual void addModuleDataToArray(ArbitraryObjectArray& arry) const = 0;
    virtual bool updateAndRenderEditorControls() = 0;
    virtual BitFlags<ParticleEmitterModuleStage> getStages() const = 0;
    virtual const char* const getName() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterModule> clone() const = 0;

    bool areAllRequiredComponentsPresent(BitFlags<ParticleComponentType> componentsToCheck) {
        return (mRequiredComponents.getBits() & componentsToCheck.getBits()) == mRequiredComponents.getBits();
    }

    CPUParticleEmitterModuleMethod getMethod() const { return mMethod; }
    BitFlags<ParticleComponentType> getRequiredComponents() const { return mRequiredComponents; }

protected:
    CPUParticleEmitterModuleMethod mMethod;
    BitFlags<ParticleComponentType> mRequiredComponents;

    // TODO: Based on https://docs.unrealengine.com/5.1/en-US/script-editor-reference-for-niagara-effects-in-unreal-engine/
    // TODO: Provided dependencies
    // TODO: Required dependencies
};

