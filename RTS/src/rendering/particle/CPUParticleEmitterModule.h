#pragma once

#include "ParticleEnumTypes.h"

#include "serialization/YmlSerializable.h"
#include "rendering/particle/ParticleEmitterVariableName.h"

class ArbitraryObjectArray;

typedef void(*CPUParticleEmitterModuleMethod)(class CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec);

enum class ParticleEmitterModuleStage : ui8 {
    EmitterUpdate = BIT(0),
    ParticleInit = BIT(1),
    ParticleUpdate = BIT(2),
};

class CPUParticleEmitterModule : public YmlSerializable
{
public:
    CPUParticleEmitterModule() = default;
    virtual ~CPUParticleEmitterModule() = default;

    virtual void refresh() = 0;
    virtual void addModuleDataToArray(ArbitraryObjectArray& arry) const = 0;
    virtual bool updateAndRenderEditorControls() = 0;
    virtual BitFlags<ParticleEmitterModuleStage> getStages() const = 0;
    virtual constexpr const char* const getName() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterModule> clone() const = 0;
    virtual bool compatableWithEmitter(const CpuParticleEmitter& emitter) const { return true; }
    virtual void addRequiredUIntVariables(FlatSet<ParticleEmitterVariableNameUInt>& uintVariables) const {}
    virtual void addRequiredFloatVariables(FlatSet<ParticleEmitterVariableNameFloat>& floatVariables) const {}
    virtual void addRequiredVec2Variables(FlatSet<ParticleEmitterVariableNameVec2>& vec2Variables) const {}
    virtual void addRequiredVec3Variables(FlatSet<ParticleEmitterVariableNameVec3>& vec3Variables) const {}

    bool areAllRequiredComponentsPresent(BitFlags<ParticleComponentType> componentsToCheck) const {
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

