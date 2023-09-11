#pragma once

#include "definitions/ParticleSystemDef.h"
#include "resources/IAssetRepository.h"

class MaterialRepository;

class ParticleSystemRepository : public IAssetRepository<ParticleSystemDef> {
public:
    static void initInstance(vio::IOManager& ioManager) {
        sInstance = std::make_unique<ParticleSystemRepository>(ioManager);
    }
    inline static ParticleSystemRepository& get() {
        assert(sInstance);
        return (ParticleSystemRepository&)*sInstance;
    }

    MaterialID getDefaultMaterialID() const { return mDefaultMaterial; }
    void setDefaultMaterialID(MaterialID id) { mDefaultMaterial = id; }

private:
    void saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter);
    bool loadParticleEmitter(ryml::ConstNodeRef node, ParticleEmitterDef& particleEmitter);

    MaterialID mDefaultMaterial = INVALID_MATERIAL_ID;
};

