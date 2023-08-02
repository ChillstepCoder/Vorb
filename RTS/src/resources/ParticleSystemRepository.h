#pragma once

#include "definitions/ParticleSystemDef.h"

class MaterialRepository;
DECL_VIO(class IOManager);

class ParticleSystemRepository
{
    friend class TileEditorPanel; //  TODO: REMOVE 
public:
    ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo);
    ~ParticleSystemRepository();

    void loadParticleEmitterFile(const vio::Path& filePath);
    void loadParticleSystemFile(const vio::Path& filePath);

    const ParticleSystemDef& getParticleSystem(ParticleSystemID id) const { return mParticleSystems[id]; }
    const ParticleSystemDef& getParticleSystem(const nString& itemName) const;
    const std::vector<ParticleSystemDef>& getAllParticleSystems() const { return mParticleSystems; }
    const std::map<nString, ParticleSystemID>& getParticleSystemNames() const { return mParticleSystemLookup; }
    const std::vector<std::unique_ptr<CPUParticleEmitterModule>>& getEmitterModules() const { return mEmitterModules; }
    const std::vector<std::unique_ptr<CPUParticleEmitterOperation>>& getEmitterOperations() const { return mEmitterOperations; }

    ParticleSystemDef* tryAddNewParticleSystem(const nString& name);

    MaterialID getDefaultMaterialID() const { return mDefaultMaterial; }
    void setDefaultMaterialID(MaterialID id) { mDefaultMaterial = id; }

private:
    vio::IOManager& mIoManager;
    MaterialRepository& mMaterialRepository;

    std::map<nString, ParticleSystemID> mParticleSystemLookup; // TODO: StrToken?
    std::vector<ParticleSystemDef> mParticleSystems;
    std::vector<ParticleEmitterDef> mParticleEmitters;
    std::vector<std::unique_ptr<CPUParticleEmitterModule>> mEmitterModules;
    std::vector<std::unique_ptr<CPUParticleEmitterOperation>> mEmitterOperations;

    MaterialID mDefaultMaterial = INVALID_MATERIAL_ID;
};

