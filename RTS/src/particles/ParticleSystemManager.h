#pragma once

#include "ParticleSystemData.h"

class ParticleSystem;
DECL_VIO(class Path);
DECL_VIO(class IOManager);

class ParticleSystemManager
{
    friend class ParticleSystemRenderer;
public:
    ParticleSystemManager(vio::IOManager& ioManager);
    ~ParticleSystemManager();

    ParticleSystem* createParticleSystem(const f32v3& position, const f32v3& initialVelocity, const nString& name);

    bool loadParticleSystemData(const vio::Path& filePath);

    void update(float deltaTime);

private:
    vio::IOManager& mIoManager;
    std::map<nString, ParticleSystemData> mParticleSystemData;
    std::vector<std::unique_ptr<ParticleSystem> > mParticleSystems;
};

