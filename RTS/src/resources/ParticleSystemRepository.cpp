#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

ParticleSystemRepository::ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo) :
    mIoManager(ioManager),
    mMaterialRepository(materialRepo) {


    mEmitterModules.resize(e_count(BuiltinCPUParticleEditorModules));

    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnBurst));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnRate));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetPosition));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetVelocity));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetColor));

    static_assert(e_count(BuiltinCPUParticleEditorModules) == 5);
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
