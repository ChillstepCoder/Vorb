#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

ParticleSystemRepository::ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo) :
    mIoManager(ioManager),
    mMaterialRepository(materialRepo) {


    mEmitterModules.reserve(e_count(BuiltinCPUParticleEditorModules));

    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnBurst));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnRate));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetPosition));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetVelocity));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetColor));
    static_assert(e_count(BuiltinCPUParticleEditorModules) == 5);

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddFloatToVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyFloatToVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddFloat>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyFloat>());

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetColor>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec4>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec2>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetFloat>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetUInt>());

    mEmitterOperations.shrink_to_fit();

    static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);
}

ParticleSystemRepository::~ParticleSystemRepository() {

}

void ParticleSystemRepository::loadParticleEmitterFile(const vio::Path& filePath)
{
    assert(false);
}

void ParticleSystemRepository::loadParticleSystemFile(const vio::Path& filePath)
{
    assert(false);
}

const ParticleSystemDef& ParticleSystemRepository::getParticleSystem(const nString& itemName) const {
    auto&& it = mParticleSystemLookup.find(itemName);
    assert(it != mParticleSystemLookup.end());
    return mParticleSystems[it->second];
}

ParticleSystemDef* ParticleSystemRepository::tryAddNewParticleSystem(const nString& name) {
    if (mParticleSystemLookup.find(name) != mParticleSystemLookup.end()) {
        return nullptr;
    }

    ParticleSystemID id = mParticleSystems.size();
    mParticleSystemLookup[name] = id;
    ParticleSystemDef& newDef = mParticleSystems.emplace_back();
    newDef.mID = id;
    return &newDef;
}
