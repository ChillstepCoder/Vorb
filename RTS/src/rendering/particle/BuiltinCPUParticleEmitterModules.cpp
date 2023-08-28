#include "stdafx.h"
#include "BuiltinCPUParticleEmitterModules.h"

#include "CpuParticleEmitter.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

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


bool updateAndRenderVariable(CPUParticleEmitterVariable& variable, const char* const label) {
    bool changed = variable.updateAndRenderTweaker(label);
    // Separator after operations
    if (variable.mOperation) {
        ImGui::Separator();
    }
    return changed;
}

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

bool CPUPEM_SpawnBurst::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mDelay, "Delay Sec");
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count");
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

bool CPUPEM_SpawnRate::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mEmitRateSec, "Spawn Rate Sec");
    changed |= updateAndRenderVariable(mModuleData.mInitialDelay, "Initial Delay Sec");
    changed |= updateAndRenderVariable(mModuleData.mSpawnCount, "Spawn Count");
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
        const f32 maxAngleFromEquator = std::get<f32>(MODULE_DATA->mMaxAngleFromRingRad.mVarData);

        // Create an arbitrary axis not parallel to the ringNormal.
        const f32v3 axis = (std::abs(ringNormal.x) < 0.5f) ? f32v3(1, 0, 0) : f32v3(0, 1, 0);

        // Calculate two tangent vectors to the ringNormal.
        const f32v3 tangent1 = glm::cross(ringNormal, axis);
        const f32v3 tangent2 = glm::cross(ringNormal, tangent1);

        // Create a random direction in the equatorial plane.
        float randomAngle = (Random::getCachedRandomf() * 2.0f - 1.0f) * maxAngleFromEquator;
        f32v3 equatorialDirection = std::cos(randomAngle) * tangent1 + std::sin(randomAngle) * tangent2;

        // Calculate final direction by interpolating between equatorialDirection and ringNormal.
        float lerpFactor = Random::getCachedRandomf();
        f32v3 direction = glm::normalize((1.0f - lerpFactor) * equatorialDirection + lerpFactor * ringNormal);

        // Randomly scale the direction to get a velocity in the required speed range.
        const float speed = Random::getCachedRandomf() * (randomSpeedRange.y - randomSpeedRange.x) + randomSpeedRange.x;
        emitter.addParticleVelocity(particleID, direction * speed);
    };
}

bool CPUPEM_RingBurst::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mSpeedRange, "Speed");
    changed |= updateAndRenderVariable(mModuleData.mMaxAngleFromRingRad, "Max Angle From Ring");
    changed |= updateAndRenderVariable(mModuleData.mRingNormal, "Ring Normal");
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

bool CPUPEM_ConeBurst::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mSpeedRange, "Speed");
    changed |= updateAndRenderVariable(mModuleData.mAngleRange, "Angle Range");
    changed |= updateAndRenderVariable(mModuleData.mDirection, "Direction");
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

CPUPEM_SetPosition::CPUPEM_SetPosition() {
    refresh();
}

void CPUPEM_SetPosition::refresh() {
    mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
        MODULE_DATA->mPositionVec3.evaluate(emitter, particleID);
        emitter.setParticlePosition(particleID, std::get<f32v3>(MODULE_DATA->mPositionVec3.mVarData));
    };
}

bool CPUPEM_SetPosition::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mPositionVec3, "Position");
}

bool CPUPEM_SetPosition::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mPositionVec3, "pos"sv);
    return true;
}

void CPUPEM_SetPosition::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mPositionVec3, "pos"sv);
}

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

bool CPUPEM_SetVelocity::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mVelocityVec3, "Velocity");
}

bool CPUPEM_SetVelocity::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mVelocityVec3, "vel"sv);
    return true;
}

void CPUPEM_SetVelocity::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mVelocityVec3, "vel"sv);
}

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

bool CPUPEM_SetColor::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mColor, "Color");
}

bool CPUPEM_SetColor::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mColor, "color"sv);
    return true;
}

void CPUPEM_SetColor::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mColor, "color"sv);
}

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

bool CPUPEM_SetScale::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mScale, "Scale");
}

bool CPUPEM_SetScale::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mScale, "scale"sv);
    return true;
}

void CPUPEM_SetScale::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mScale, "scale"sv);
}

CPUPEM_SetPositionFromShape::CPUPEM_SetPositionFromShape() {
    refresh();
}

void CPUPEM_SetPositionFromShape::refresh() {

    switch (mModuleData.mShapeType) {
        case ShapeType::Sphere:
            mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
                MODULE_DATA->mRadius.evaluate(emitter, particleID);
                f32 theta = 2.f * M_PIF * Random::getCachedRandomf(); // azimuthal angle
                f32 phi = acosf(2.f * Random::getCachedRandomf() - 1.f); // polar angle
                f32 r = std::get<f32>(MODULE_DATA->mRadius.mVarData) * Random::getCachedRandomf(); // cube root to ensure points are uniformly distributed

                f32v3 point;
                point.x = r * sin(phi) * cos(theta);
                point.y = r * sin(phi) * sin(theta);
                point.z = r * cos(phi);
                emitter.setParticlePosition(particleID, point);
            };
            break;
        case ShapeType::Box:
            mMethod = [](CpuParticleEmitter& emitter, int particleID, void* data, f32 elapsedSec) {
                assert(false);
            };
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_count(ShapeType) == 2);
}

bool CPUPEM_SetPositionFromShape::updateAndRenderEditorControls() {
    bool changed = false;
    changed |= updateAndRenderVariable(mModuleData.mRadius, "Radius");

    const char* itemNames[e_count(ShapeType)] = {
        "Sphere",
        "Box"
    };
    static_assert(e_count(ShapeType) == 2);

    changed |= ImGui::Combo("Shape", (int*)&mModuleData.mShapeType, itemNames, e_count(ShapeType));

    return changed;
}

bool CPUPEM_SetPositionFromShape::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mRadius, "radius"sv);
    int result;
    node["shape"] >> result;
    mModuleData.mShapeType = (ShapeType)result;
    return true;
}

void CPUPEM_SetPositionFromShape::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mRadius, "radius"sv);
    node["shape"] << (int)mModuleData.mShapeType;
}

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

bool CPUPEM_ApplyForce::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mForce, "Force");
}

bool CPUPEM_ApplyForce::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mForce, "force"sv);
    return true;
}

void CPUPEM_ApplyForce::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mForce, "force"sv);
}

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

bool CPUPEM_DragForce::updateAndRenderEditorControls() {
    return updateAndRenderVariable(mModuleData.mDragFactor, "Drag Factor");
}

bool CPUPEM_DragForce::loadFromYml(ryml::ConstNodeRef node) {
    LOAD_VAR(mDragFactor, "drag"sv);
    return true;
}

void CPUPEM_DragForce::saveYmlData(ryml::NodeRef node) const {
    SAVE_VAR(mDragFactor, "drag"sv);
}