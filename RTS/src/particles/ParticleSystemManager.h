#pragma once

#include "ParticleSystemData.h"

class ParticleSystem;
typedef std::vector<std::unique_ptr<ParticleSystem> > ParticleSystemArray;
DECL_VIO(class Path);
DECL_VIO(class IOManager);

class ParticleSystemManager
{
    friend class ParticleSystemRenderer;
public:
    ParticleSystemManager(vio::IOManager& ioManager);
    ~ParticleSystemManager();

    ParticleSystem* createParticleSystem(const f32v3& position, const f32v3& direction, const nString& name);

    bool loadParticleSystemData(const vio::Path& filePath);

    void update(const f32v2& playerPos);

private:
    vio::IOManager& mIoManager;
    std::map<nString, ParticleSystemData> mParticleSystemData;
    std::map<nString, ParticleSystemArray> mParticleSystems;
    std::vector<std::pair<ui32, nString> > mSystemLayerSortOrder;
};

