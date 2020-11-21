#include "stdafx.h"
#include "ParticleSystemManager.h"

#include "ParticleSystem.h"

#include <Vorb/io/IOManager.h>

ParticleSystemManager::ParticleSystemManager(vio::IOManager& ioManager) :
    mIoManager(ioManager)
{

}

ParticleSystemManager::~ParticleSystemManager()
{

}

ParticleSystem* ParticleSystemManager::createParticleSystem(const f32v3& position, const f32v3& initialVelocity, const nString& name) {

    auto&& it = mParticleSystemData.find(name);
    assert(it != mParticleSystemData.end());

    mParticleSystems.emplace_back(std::make_unique<ParticleSystem>(position, initialVelocity, it->second));
    return mParticleSystems.back().get();
}

bool ParticleSystemManager::loadParticleSystemData(const vio::Path& filePath) {
    // Read file
    nString data;
    mIoManager.readFileToString(filePath, data);
    if (data.empty()) return false;

    // Convert to YAML
    keg::ReadContext context;
    context.env = keg::getGlobalEnvironment();
    context.reader.init(data.c_str());
    keg::Node rootObject = context.reader.getFirst();
    if (keg::getType(rootObject) != keg::NodeType::MAP) {
        context.reader.dispose();
        return false;
    }

    auto f = makeFunctor([&](Sender, const nString& key, keg::Node value) {
        ParticleSystemData systemData;
        keg::Error error = keg::parse((ui8*)&systemData, value, context, &KEG_GLOBAL_TYPE(ParticleSystemData));
        assert(error == keg::Error::NONE);

        mParticleSystemData[key] = systemData;
    });

    context.reader.forAllInMap(rootObject, &f);
    context.reader.dispose();

    return true;
}

void ParticleSystemManager::update(float deltaTime) {

    for (unsigned i = 0; i < mParticleSystems.size();) {
        if (mParticleSystems[i]->update(deltaTime)) {
            if (i != mParticleSystems.size() - 1) {
                mParticleSystems[i] = std::move(mParticleSystems.back());
            }
            mParticleSystems.pop_back();
        }
        else {
            ++i;
        }
    }
}
