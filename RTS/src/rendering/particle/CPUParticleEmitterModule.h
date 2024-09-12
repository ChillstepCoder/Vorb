#pragma once

#include "ParticleEnumTypes.h"

#include "serialization/YmlSerializable.h"
#include "rendering/particle/ParticleEmitterVariableName.h"

class ArbitraryObjectArray;
class ParticleEmitterDef;

typedef void(*CPUParticleEmitterModuleMethod)(class CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec);

enum class ParticleEmitterModuleStage : ui8 {
    EmitterUpdate = BIT(0),
    ParticleInit = BIT(1),
    ParticleUpdate = BIT(2),
};

struct RequiredEmitterVariables {
    FlatSet<ParticleEmitterVariableNameUInt>& uintVariables;
    FlatSet<ParticleEmitterVariableNameFloat>& floatVariables;
    FlatSet<ParticleEmitterVariableNameVec2>& vec2Variables;
    FlatSet<ParticleEmitterVariableNameVec3>& vec3Variables;
};;

class CPUParticleEmitterModule : public YmlSerializable
{
public:
    CPUParticleEmitterModule() = default;
    virtual ~CPUParticleEmitterModule() = default;

    virtual void refresh() = 0;
    virtual void addModuleDataToArray(ArbitraryObjectArray& arry) const = 0;
    virtual bool updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) = 0;
    virtual BitFlags<ParticleEmitterModuleStage> getStages() const = 0;
    virtual constexpr const char* const getName() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterModule> clone() const = 0;
    virtual bool compatableWithEmitter(const CpuParticleEmitter& emitter) const { return true; }
    virtual void addRequiredVariables(RequiredEmitterVariables& variables) const {}

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

