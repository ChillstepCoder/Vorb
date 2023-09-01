#pragma once

#include "definitions/ParticleSystemDef.h"
#include "resources/IAssetRepository.h"

class MaterialRepository;

class ParticleSystemRepository : public IAssetRepository<ParticleSystemDef> {
public:
    ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo);
    ~ParticleSystemRepository();

    void loadParticleSystemFile(const vio::Path& filePath);
    bool saveParticleSystem(const ParticleSystemDef& particleSystem);

    MaterialID getDefaultMaterialID() const { return mDefaultMaterial; }
    void setDefaultMaterialID(MaterialID id) { mDefaultMaterial = id; }

private:
    void saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter);
    bool loadParticleEmitter(ryml::ConstNodeRef node, ParticleEmitterDef& particleEmitter);

    MaterialRepository& mMaterialRepository;

    MaterialID mDefaultMaterial = INVALID_MATERIAL_ID;
};

