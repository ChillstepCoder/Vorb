#pragma once

#include "definitions/ParticleSystemDef.h"
#include "resources/IAssetRepository.h"

class MaterialRepository;

class ParticleSystemRepository : public IAssetRepository
{
    friend class TileEditorPanel; //  TODO: REMOVE 
public:
    ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo);
    ~ParticleSystemRepository();

    void loadParticleSystemFile(const vio::Path& filePath);
    bool saveParticleSystem(const ParticleSystemDef& particleSystem);

    const ParticleSystemDef& getParticleSystem(ParticleSystemID id) const { return mParticleSystems[id]; }
    const ParticleSystemDef& getParticleSystem(const nString& itemName) const;
    const std::vector<ParticleSystemDef>& getAllParticleSystems() const { return mParticleSystems; }
    const std::map<nString, ParticleSystemID>& getParticleSystemNames() const { return mParticleSystemLookup; }

    ParticleSystemDef* tryAddNewParticleSystem(const nString& name);

    MaterialID getDefaultMaterialID() const { return mDefaultMaterial; }
    void setDefaultMaterialID(MaterialID id) { mDefaultMaterial = id; }

private:
    void saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter);
    bool loadParticleEmitter(ryml::ConstNodeRef node, ParticleEmitterDef& particleEmitter);

    MaterialRepository& mMaterialRepository;

    std::map<nString, ParticleSystemID> mParticleSystemLookup; // TODO: StrToken?
    std::vector<ParticleSystemDef> mParticleSystems;
    std::vector<ParticleEmitterDef> mParticleEmitters;

    MaterialID mDefaultMaterial = INVALID_MATERIAL_ID;
};

