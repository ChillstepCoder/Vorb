#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#include <imgui.h>


#include "math/Random.h"

#define MODULE_DATA static_cast<ModuleData*>(data)

#define SAVE_VAR(var, name) \
mModuleData.var.saveYmlData(node, name)

#define LOAD_VAR(var, name) \
mModuleData.var.loadFromYml(node, name)

#define SAVE_ONE_VAR(var1, name1) \
beginMap(writer); \
saveNested(writer, name1, YML_SAVE_LAMBDA(mModuleData.var1));\
endMap(writer);

#define SAVE_TWO_VAR(var1, name1, var2, name2) \
beginMap(writer); \
saveNested(writer, name1, YML_SAVE_LAMBDA(mModuleData.var1));\
saveNested(writer, name2, YML_SAVE_LAMBDA(mModuleData.var2)); \
endMap(writer);

#define SAVE_THREE_VAR(var1, name1, var2, name2, var3, name3) \
beginMap(writer); \
saveNested(writer, name1, YML_SAVE_LAMBDA(mModuleData.var1)); \
saveNested(writer, name2, YML_SAVE_LAMBDA(mModuleData.var2)); \
saveNested(writer, name3, YML_SAVE_LAMBDA(mModuleData.var3)); \
endMap(writer);

// Helper
bool updateAndRenderVariable(CPUParticleEmitterParameter& variable, const char* const label, const ParticleEmitterDef& parentEmitter) {
    bool changed = variable.updateAndRenderTweaker(label, parentEmitter);
    // Separator after operations
    if (variable.mOperation) {
        ImGui::Separator();
    }
    return changed;
}

// ====================================================================================================
// CPUPEM_SpawnBurst
// ====================================================================================================
#pragma region CPUPEM_SpawnBurst
CPUPEM_SpawnBurst::CPUPEM_SpawnBurst() {
    refresh();
}

void CPUPEM_SpawnBurst::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        ModuleData* moduleData = MODULE_DATA;
        if (moduleData->mFired) return;

        moduleData->mDelay.evaluate(emitter, particleID);
        f32 delay = std::get<f32>(moduleData->mDelay.mVarData);
        if (emitter.getTotalElapsedSec() >= delay) {
            moduleData->mFired = true;
            moduleData->mSpawnCount.evaluate(emitter, particleID);
            ui32 spawnCount = std::get<ui32>(moduleData->mSpawnCount.mVarData);
            emitter.emitParticles(spawnCount);
        }
    };
}

bool CPUPEM_SpawnBurst::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mDelay, "Delay Sec", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count", parentEmitter);
    return changed;
}

bool CPUPEM_SpawnBurst::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mDelay, "delay"sv);
    LOAD_VAR(mSpawnCount, "spawn_count"sv);
    return true;
}

void CPUPEM_SpawnBurst::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mDelay, "delay"sv);
    SAVE_VAR(mSpawnCount, "spawn_count"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SpawnRate
// ====================================================================================================
#pragma region CPUPEM_SpawnRate
CPUPEM_SpawnRate::CPUPEM_SpawnRate() {
    refresh();
}

void CPUPEM_SpawnRate::refresh() {
    
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        ModuleData* moduleData = MODULE_DATA;

        if (moduleData->mNextEmitTime == -1.0f) {
            moduleData->mInitialDelay.evaluate(emitter, particleID);
            moduleData->mNextEmitTime = std::get<f32>(moduleData->mInitialDelay.mVarData);
        }

        const f32 timeDiff = emitter.getTotalElapsedSec() - moduleData->mNextEmitTime;
        if (timeDiff >= 0.0f) {
            moduleData->mSpawnCount.evaluate(emitter, particleID);
            moduleData->mEmitRateSec.evaluate(emitter, particleID);
            emitter.emitParticles(std::get<ui32>(moduleData->mSpawnCount.mVarData));
            moduleData->mNextEmitTime = emitter.getTotalElapsedSec() + std::get<f32>(moduleData->mEmitRateSec.mVarData) - timeDiff;
        }
    };
}

bool CPUPEM_SpawnRate::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mEmitRateSec, "Spawn Rate Sec", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mInitialDelay, "Initial Delay Sec", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count", parentEmitter);
    return changed;
}

bool CPUPEM_SpawnRate::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mEmitRateSec, "emit_rate"sv);
    LOAD_VAR(mInitialDelay, "initial_delay"sv);
    LOAD_VAR(mSpawnCount, "spawn_count"sv);
    return true;
}

void CPUPEM_SpawnRate::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mEmitRateSec, "emit_rate"sv);
    SAVE_VAR(mInitialDelay, "initial_delay"sv);
    SAVE_VAR(mSpawnCount, "spawn_count"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_RingBurst
// ====================================================================================================
#pragma region CPUPEM_RingBurst
CPUPEM_RingBurst::CPUPEM_RingBurst() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_RingBurst::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mSpeedRange.evaluate(emitter, particleID);
        MODULE_DATA->mMaxAngleFromRingRad.evaluate(emitter, particleID);
        MODULE_DATA->mRingNormal.evaluate(emitter, particleID);

        const f32v3 ringNormal = glm::normalize(std::get<f32v3>(MODULE_DATA->mRingNormal.mVarData));
        const f32v2 randomSpeedRange = std::get<f32v2>(MODULE_DATA->mSpeedRange.mVarData);
        const f32 maxAngleFromEquator = DEG_TO_RAD(std::get<f32>(MODULE_DATA->mMaxAngleFromRingRad.mVarData));

        // Create an arbitrary axis not parallel to the ringNormal.
        const f32v3 axis = (std::abs(ringNormal.x) < 0.5f) ? f32v3(1, 0, 0) : f32v3(0, 1, 0);
        f32v3 tangent = glm::cross(ringNormal, axis);
        f32 randomCircleAngle = Random::getCachedRandomf() * M_2_PIF;
        glm::quat rotQuat = glm::angleAxis(randomCircleAngle, ringNormal);
        tangent = rotQuat * tangent;
        
        const f32v3 tangent2 = glm::cross(tangent, ringNormal);

        // Create a random direction in the equatorial plane.
        float randomAngle = (Random::getCachedRandomf() * 2.0f - 1.0f) * maxAngleFromEquator;
        f32v3 launchDir = glm::angleAxis(randomAngle, tangent2) * tangent;

        // Randomly scale the direction to get a velocity in the required speed range.
        const float speed = Random::getCachedRandomf() * (randomSpeedRange.y - randomSpeedRange.x) + randomSpeedRange.x;
        emitter.addParticleVelocity(particleID, launchDir * speed);
    };
}

bool CPUPEM_RingBurst::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mSpeedRange, "Speed", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mMaxAngleFromRingRad, "Max Angle From Ring", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mRingNormal, "Ring Normal", parentEmitter);
    return changed;
}

bool CPUPEM_RingBurst::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mSpeedRange, "speed_range"sv);
    LOAD_VAR(mMaxAngleFromRingRad, "max_angle"sv);
    LOAD_VAR(mRingNormal, "ring_normal"sv);
    return true;
}

void CPUPEM_RingBurst::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mSpeedRange, "speed_range"sv);
    SAVE_VAR(mMaxAngleFromRingRad, "max_angle"sv);
    SAVE_VAR(mRingNormal, "ring_normal"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_ConeBurst
// ====================================================================================================
#pragma region CPUPEM_ConeBurst
CPUPEM_ConeBurst::CPUPEM_ConeBurst() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_ConeBurst::refresh() {
    // https://gamedev.stackexchange.com/questions/26789/random-vector-within-a-cone
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mSpeedRange.evaluate(emitter, particleID);
        MODULE_DATA->mAngleRange.evaluate(emitter, particleID);
        MODULE_DATA->mDirection.evaluate(emitter, particleID);

        f32v3 direction = glm::normalize(std::get<f32v3>(MODULE_DATA->mDirection.mVarData));
        const f32v2 randomSpeedRange = std::get<f32v2>(MODULE_DATA->mSpeedRange.mVarData);
        const f32v2 randomAngleRange = std::get<f32v2>(MODULE_DATA->mAngleRange.mVarData);

        const f32 speed = Random::getCachedRandomf() * (randomSpeedRange.y - randomSpeedRange.x) + randomSpeedRange.x;
        const f32 maxAngle = Random::getCachedRandomf() * (randomAngleRange.y - randomAngleRange.x) + randomAngleRange.x;

        // Improved distribution  (apparently? looks bad still)
        float theta = std::acos(1.0f - Random::getCachedRandomf() * (1.0f - std::cos(maxAngle)));
        float phi = Random::getCachedRandomf() * 2.0f * glm::pi<float>();
        
        f32 sinTheta = sin(theta);
        // Convert (theta, phi) to a direction vector in spherical coordinates
        f32v3 sample(cos(phi) * sinTheta, sin(phi) * sinTheta, cos(theta));

        f32v3 up(0.0f, 0.0f, 1.0f);

        // Check if direction is nearly parallel to the "up" vector
        f32 d = glm::dot(direction, up);
        if (abs(d) > 0.9999f) {
            up = f32v3(-1.0f, 0.0f, 0.0f);
        }

        f32v3 right = glm::normalize(glm::cross(direction, up));
        up = glm::cross(right, direction);
        glm::mat3 rotation(right, up, direction);
        direction = rotation * sample;

        emitter.addParticleVelocity(particleID, direction * speed);
    };
}

bool CPUPEM_ConeBurst::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mSpeedRange, "Speed", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mAngleRange, "Angle Range", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mDirection, "Direction", parentEmitter);
    return changed;
}

bool CPUPEM_ConeBurst::loadFromYml(ryml::ConstNodeRef node)  {
    LOAD_VAR(mSpeedRange, "speed_range"sv);
    LOAD_VAR(mAngleRange, "angle_range"sv);
    LOAD_VAR(mDirection, "dir"sv);
    return true;
}

void CPUPEM_ConeBurst::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mSpeedRange, "speed_range"sv);
    SAVE_VAR(mAngleRange, "angle_range"sv);
    SAVE_VAR(mDirection, "dir"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetPosition
// ====================================================================================================
#pragma region CPUPEM_SetPosition
CPUPEM_SetPosition::CPUPEM_SetPosition() {
    refresh();
}

void CPUPEM_SetPosition::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

bool CPUPEM_SetPosition::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mPositionVec3, "Position", parentEmitter);
}

bool CPUPEM_SetPosition::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mPositionVec3, "pos"sv);
    return true;
}

void CPUPEM_SetPosition::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mPositionVec3, "pos"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetVelocity
// ====================================================================================================
#pragma region CPUPEM_SetVelocity
CPUPEM_SetVelocity::CPUPEM_SetVelocity() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_SetVelocity::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mVelocityVec3.evaluate(emitter, particleID);
        emitter.setParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mVelocityVec3.mVarData));
    };
}

bool CPUPEM_SetVelocity::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mVelocityVec3, "Velocity", parentEmitter);
}

bool CPUPEM_SetVelocity::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVelocityVec3, "vel"sv);
    return true;
}

void CPUPEM_SetVelocity::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVelocityVec3, "vel"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetRotation
// ====================================================================================================
#pragma region CPUPEM_SetRotation
CPUPEM_SetRotation::CPUPEM_SetRotation() {
    mRequiredComponents |= ParticleComponentType::Rotation;
    refresh();
}

void CPUPEM_SetRotation::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mRotationVec2.evaluate(emitter, particleID);
        emitter.setParticleRotation(particleID, std::get<f32v2>(MODULE_DATA->mRotationVec2.mVarData));
    };
}

bool CPUPEM_SetRotation::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mRotationVec2, "Rotation", parentEmitter);
}

bool CPUPEM_SetRotation::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mRotationVec2, "rot"sv);
    return true;
}

void CPUPEM_SetRotation::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mRotationVec2, "rot"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetColor
// ====================================================================================================
#pragma region CPUPEM_SetColor
CPUPEM_SetColor::CPUPEM_SetColor() {
    mRequiredComponents |= ParticleComponentType::Color;
    refresh();
}

void CPUPEM_SetColor::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleColor(particleID, std::get<color4>(MODULE_DATA->mColor.mVarData));
    };
}

bool CPUPEM_SetColor::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mColor, "Color", parentEmitter);
}

bool CPUPEM_SetColor::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mColor, "color"sv);
    return true;
}

void CPUPEM_SetColor::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mColor, "color"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetHdrColor
// ====================================================================================================
#pragma region CPUPEM_SetHdrColor
CPUPEM_SetHdrColor::CPUPEM_SetHdrColor() {
    mRequiredComponents |= ParticleComponentType::HDRColor;
    refresh();
}

void CPUPEM_SetHdrColor::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mColor.evaluate(emitter, particleID);
        emitter.setParticleHDRColor(particleID, std::get<f32v4>(MODULE_DATA->mColor.mVarData));
    };
}

bool CPUPEM_SetHdrColor::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mColor, "HDR Color", parentEmitter);
}

bool CPUPEM_SetHdrColor::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mColor, "color"sv);
    return true;
}

void CPUPEM_SetHdrColor::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mColor, "color"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetScale
// ====================================================================================================
#pragma region CPUPEM_SetScale
CPUPEM_SetScale::CPUPEM_SetScale() {
    mRequiredComponents |= ParticleComponentType::Scale;
    refresh();
}

void CPUPEM_SetScale::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mScale.evaluate(emitter, particleID);
        emitter.setParticleScale(particleID, std::get<f32v2>(MODULE_DATA->mScale.mVarData));
    };
}

bool CPUPEM_SetScale::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mScale, "Scale", parentEmitter);
}

bool CPUPEM_SetScale::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mScale, "scale"sv);
    return true;
}

void CPUPEM_SetScale::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mScale, "scale"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetUIntVar
// ====================================================================================================
#pragma region CPUPEM_SetUIntVar

CPUPEM_SetUIntVar::CPUPEM_SetUIntVar() {
    refresh();
}

void CPUPEM_SetUIntVar::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        //MODULE_DATA->mVariable.evaluate(emitter, particleID); // TODO: Can we skip evaluate for variable names? Are there any operations that can output?
        MODULE_DATA->mValue.evaluate(emitter, particleID);
        emitter.setUIntVariable(std::get<ParticleEmitterVariableNameUInt>(MODULE_DATA->mVariable.mVarData), particleID, std::get<ui32>(MODULE_DATA->mValue.mVarData));
    };
}

bool CPUPEM_SetUIntVar::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = updateAndRenderVariable(mModuleData.mVariable, "Var", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mValue, "Value", parentEmitter);
    return changed;
}

bool CPUPEM_SetUIntVar::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVariable, "name"sv);
    LOAD_VAR(mValue, "value"sv);
    return true;
}

void CPUPEM_SetUIntVar::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVariable, "name"sv);
    SAVE_VAR(mValue, "value"sv);
}

bool CPUPEM_SetUIntVar::compatableWithEmitter(const CpuParticleEmitter& emitter) const {
    ParticleEmitterVariableNameUInt varName = std::get<ParticleEmitterVariableNameUInt>(mModuleData.mVariable.mVarData);
    if (varName == ParticleEmitterVariableNameUInt::INVALID) return false;
    return emitter.hasUIntVariable(varName);
}

void CPUPEM_SetUIntVar::addRequiredUIntVariables(FlatSet<ParticleEmitterVariableNameUInt>& uintVariables) const {
    ParticleEmitterVariableNameUInt varName = std::get<ParticleEmitterVariableNameUInt>(mModuleData.mVariable.mVarData);
    if (varName != ParticleEmitterVariableNameUInt::INVALID) {
        uintVariables.insert(varName);
    }
}

#pragma endregion


// ====================================================================================================
// CPUPEM_SetFloatVar
// ====================================================================================================
#pragma region CPUPEM_SetFloatVar
CPUPEM_SetFloatVar::CPUPEM_SetFloatVar() {
    refresh();
}

void CPUPEM_SetFloatVar::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        //MODULE_DATA->mVariable.evaluate(emitter, particleID); // TODO: Can we skip evaluate for variable names? Are there any operations that can output?
        MODULE_DATA->mValue.evaluate(emitter, particleID);
        emitter.setFloatVariable(std::get<ParticleEmitterVariableNameFloat>(MODULE_DATA->mVariable.mVarData), particleID, std::get<f32>(MODULE_DATA->mValue.mVarData));
    };
}

bool CPUPEM_SetFloatVar::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = updateAndRenderVariable(mModuleData.mVariable, "Var", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mValue, "Value", parentEmitter);
    return changed;
}

bool CPUPEM_SetFloatVar::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVariable, "name"sv);
    LOAD_VAR(mValue, "value"sv);
    return true;
}

void CPUPEM_SetFloatVar::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVariable, "name"sv);
    SAVE_VAR(mValue, "value"sv);
}

bool CPUPEM_SetFloatVar::compatableWithEmitter(const CpuParticleEmitter& emitter) const {
    ParticleEmitterVariableNameFloat varName = std::get<ParticleEmitterVariableNameFloat>(mModuleData.mVariable.mVarData);
    if (varName == ParticleEmitterVariableNameFloat::INVALID) return false;
    return emitter.hasFloatVariable(varName);
}

void CPUPEM_SetFloatVar::addRequiredFloatVariables(FlatSet<ParticleEmitterVariableNameFloat>& floatVariables) const {
    ParticleEmitterVariableNameFloat varName = std::get<ParticleEmitterVariableNameFloat>(mModuleData.mVariable.mVarData);
    if (varName != ParticleEmitterVariableNameFloat::INVALID) {
        floatVariables.insert(varName);
    }
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetVec2Var
// ====================================================================================================
#pragma region CPUPEM_SetVec2Var

CPUPEM_SetVec2Var::CPUPEM_SetVec2Var() {
    refresh();
}

void CPUPEM_SetVec2Var::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        //MODULE_DATA->mVariable.evaluate(emitter, particleID); // TODO: Can we skip evaluate for variable names? Are there any operations that can output?
        MODULE_DATA->mValue.evaluate(emitter, particleID);
        emitter.setVec2Variable(std::get<ParticleEmitterVariableNameVec2>(MODULE_DATA->mVariable.mVarData), particleID, std::get<f32v2>(MODULE_DATA->mValue.mVarData));
    };
}

bool CPUPEM_SetVec2Var::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = updateAndRenderVariable(mModuleData.mVariable, "Var", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mValue, "Value", parentEmitter);
    return changed;
}

bool CPUPEM_SetVec2Var::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVariable, "name"sv);
    LOAD_VAR(mValue, "value"sv);
    return true;
}

void CPUPEM_SetVec2Var::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVariable, "name"sv);
    SAVE_VAR(mValue, "value"sv);
}

bool CPUPEM_SetVec2Var::compatableWithEmitter(const CpuParticleEmitter& emitter) const {
    ParticleEmitterVariableNameVec2 varName = std::get<ParticleEmitterVariableNameVec2>(mModuleData.mVariable.mVarData);
    if (varName == ParticleEmitterVariableNameVec2::INVALID) return false;
    return emitter.hasVec2Variable(varName);
}

void CPUPEM_SetVec2Var::addRequiredVec2Variables(FlatSet<ParticleEmitterVariableNameVec2>& floatVariables) const {
    ParticleEmitterVariableNameVec2 varName = std::get<ParticleEmitterVariableNameVec2>(mModuleData.mVariable.mVarData);
    if (varName != ParticleEmitterVariableNameVec2::INVALID) {
        floatVariables.insert(varName);
    }
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetVec3Var
// ====================================================================================================
#pragma region CPUPEM_SetVec3Var

CPUPEM_SetVec3Var::CPUPEM_SetVec3Var() {
    refresh();
}

void CPUPEM_SetVec3Var::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        //MODULE_DATA->mVariable.evaluate(emitter, particleID); // TODO: Can we skip evaluate for variable names? Are there any operations that can output?
        MODULE_DATA->mValue.evaluate(emitter, particleID);
        emitter.setVec3Variable(std::get<ParticleEmitterVariableNameVec3>(MODULE_DATA->mVariable.mVarData), particleID, std::get<f32v3>(MODULE_DATA->mValue.mVarData));
    };
}

bool CPUPEM_SetVec3Var::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = updateAndRenderVariable(mModuleData.mVariable, "Var", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mValue, "Value", parentEmitter);
    return changed;
}

bool CPUPEM_SetVec3Var::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVariable, "name"sv);
    LOAD_VAR(mValue, "value"sv);
    return true;
}

void CPUPEM_SetVec3Var::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVariable, "name"sv);
    SAVE_VAR(mValue, "value"sv);
}

bool CPUPEM_SetVec3Var::compatableWithEmitter(const CpuParticleEmitter& emitter) const {
    ParticleEmitterVariableNameVec3 varName = std::get<ParticleEmitterVariableNameVec3>(mModuleData.mVariable.mVarData);
    if (varName == ParticleEmitterVariableNameVec3::INVALID) return false;
    return emitter.hasVec3Variable(varName);
}

void CPUPEM_SetVec3Var::addRequiredVec3Variables(FlatSet<ParticleEmitterVariableNameVec3>& floatVariables) const {
    ParticleEmitterVariableNameVec3 varName = std::get<ParticleEmitterVariableNameVec3>(mModuleData.mVariable.mVarData);
    if (varName != ParticleEmitterVariableNameVec3::INVALID) {
        floatVariables.insert(varName);
    }
}
#pragma endregion


// ====================================================================================================
// CPUPEM_SetLifespan
// ====================================================================================================
#pragma region CPUPEM_SetLifespan
CPUPEM_SetLifespan::CPUPEM_SetLifespan() {
    mRequiredComponents |= ParticleComponentType::Lifespan;
    refresh();
}

void CPUPEM_SetLifespan::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mLifespan.evaluate(emitter, particleID);
        emitter.setParticleLifespan(particleID, std::get<f32>(MODULE_DATA->mLifespan.mVarData));
    };
}

bool CPUPEM_SetLifespan::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mLifespan, "Lifetime", parentEmitter);
}

bool CPUPEM_SetLifespan::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mLifespan, "life"sv);
    return true;
}

void CPUPEM_SetLifespan::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mLifespan, "life"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_MultiplyScale
// ====================================================================================================
#pragma region CPUPEM_MultiplyScale
CPUPEM_MultiplyScale::CPUPEM_MultiplyScale() {
    mRequiredComponents |= ParticleComponentType::Scale;
    refresh();
}

void CPUPEM_MultiplyScale::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mScale.evaluate(emitter, particleID);
        emitter.multiplyParticleScale(particleID, std::get<f32v2>(MODULE_DATA->mScale.mVarData));
    };
}

bool CPUPEM_MultiplyScale::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mScale, "Scale", parentEmitter);
}

bool CPUPEM_MultiplyScale::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mScale, "scale"sv);
    return true;
}

void CPUPEM_MultiplyScale::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mScale, "scale"sv);
}
#pragma endregion



// ====================================================================================================
// CPUPEM_MultiplyVelocity
// ====================================================================================================
#pragma region CPUPEM_MultiplyVelocity
CPUPEM_MultiplyVelocity::CPUPEM_MultiplyVelocity() {
    mRequiredComponents |= ParticleComponentType::Scale;
    refresh();
}

void CPUPEM_MultiplyVelocity::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mScale.evaluate(emitter, particleID);
        emitter.multiplyParticleVelocity(particleID, std::get<f32v3>(MODULE_DATA->mScale.mVarData));
    };
}

bool CPUPEM_MultiplyVelocity::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mScale, "Scale", parentEmitter);
}

bool CPUPEM_MultiplyVelocity::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mScale, "scale"sv);
    return true;
}

void CPUPEM_MultiplyVelocity::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mScale, "scale"sv);
}
#pragma endregion

// ====================================================================================================
// CPUPEM_ApplyForce
// ====================================================================================================
#pragma region CPUPEM_ApplyForce
CPUPEM_ApplyForce::CPUPEM_ApplyForce() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_ApplyForce::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mForce.evaluate(emitter, particleID);
        f32v3 velocity = emitter.getParticleVelocity(particleID);
        velocity += elapsedSec * std::get<f32v3>(MODULE_DATA->mForce.mVarData);
        emitter.setParticleVelocity(particleID, velocity);
    };
}

bool CPUPEM_ApplyForce::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mForce, "Force", parentEmitter);
}

bool CPUPEM_ApplyForce::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mForce, "force"sv);
    return true;
}

void CPUPEM_ApplyForce::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mForce, "force"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_DragForce
// ====================================================================================================
#pragma region CPUPEM_DragForce
CPUPEM_DragForce::CPUPEM_DragForce() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_DragForce::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mDragFactor.evaluate(emitter, particleID);
        f32v3 velocity = emitter.getParticleVelocity(particleID);
        velocity *= MathUtil::dragForceWithDeltaTime(std::get<f32>(MODULE_DATA->mDragFactor.mVarData), elapsedSec);
        emitter.setParticleVelocity(particleID, velocity);
    };
}

bool CPUPEM_DragForce::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    return updateAndRenderVariable(mModuleData.mDragFactor, "Drag Factor", parentEmitter);
}

bool CPUPEM_DragForce::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mDragFactor, "drag"sv);
    return true;
}

void CPUPEM_DragForce::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mDragFactor, "drag"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_Turbulence
// ====================================================================================================
#pragma region CPUPEM_Turbulence
CPUPEM_Turbulence::CPUPEM_Turbulence() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    refresh();
}

void CPUPEM_Turbulence::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mScaleFactor.evaluate(emitter, particleID);
        MODULE_DATA->mDirOffset.evaluate(emitter, particleID);
        f32v3 velocity = emitter.getParticleVelocity(particleID);
        // Experiment with all 3 values the same
        const f32v3 rndVec = f32v3(Random::getCachedRandomf() * 2.0f - 1.0f);
        const f32v3 dirVec = std::get<f32v3>(MODULE_DATA->mDirOffset.mVarData);
        f32v3 scaledVec = (rndVec + dirVec) * std::get<f32v3>(MODULE_DATA->mScaleFactor.mVarData);
        emitter.setParticleVelocity(particleID, velocity + scaledVec * elapsedSec);
    };
}

bool CPUPEM_Turbulence::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mScaleFactor, "Scale Factor", parentEmitter);
    changed |= updateAndRenderVariable(mModuleData.mDirOffset, "Dir Offset", parentEmitter);
    return changed;
}

bool CPUPEM_Turbulence::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mScaleFactor, "scale_fac"sv);
    LOAD_VAR(mDirOffset, "dir_off"sv);
    return true;
}

void CPUPEM_Turbulence::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mScaleFactor, "scale_fac"sv);
    SAVE_VAR(mDirOffset, "dir_off"sv);
}
#pragma endregion


// ====================================================================================================
// CPUPEM_OrientToVelocity
// ====================================================================================================
#pragma region CPUPEM_OrientToVelocity
CPUPEM_OrientToVelocity::CPUPEM_OrientToVelocity() {
    mRequiredComponents |= ParticleComponentType::Velocity;
    mRequiredComponents |= ParticleComponentType::Rotation;
    refresh();
}

void CPUPEM_OrientToVelocity::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        const f32v3 velocity = emitter.getParticleVelocity(particleID);
        // No velocity means leave the rotation the same
        if (glm::length2(velocity) < 0.0001f) {
            return;
        }
        // Calculate Pitch: The rotation needed to align the particle with the world Z-axis
        const f32 pitch = atan2(velocity.x, velocity.z);

        // Calculate Roll: The rotation about the world's X-axis
        // Essentially, this would be the angle the velocity vector makes with the XZ plane.
        const f32 roll = -atan2(velocity.y, sqrt(velocity.x * velocity.x + velocity.z * velocity.z));

        emitter.setParticleRotation(particleID, f32v2(roll, pitch));
    };
}

bool CPUPEM_OrientToVelocity::updateAndRenderEditorControls(const ParticleEmitterDef& parentEmitter) {
    bool changed = false;
   
    //  TODO
    return changed;
}

bool CPUPEM_OrientToVelocity::loadFromYml(ryml::ConstNodeRef node) {
   // LOAD_VAR(mScaleFactor, "scale_fac"sv);
   // LOAD_VAR(mDirOffset, "dir_off"sv);
    return true;
}

void CPUPEM_OrientToVelocity::saveYmlData(ryml::NodeRef node) const {
  //  SAVE_VAR(mScaleFactor, "scale_fac"sv);
  //  SAVE_VAR(mDirOffset, "dir_off"sv);
}
#pragma endregion