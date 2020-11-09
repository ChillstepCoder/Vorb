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
    if (it == mParticleSystemData.end()) {
        return nullptr;
    }

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
    });

    context.reader.forAllInMap(rootObject, &f);
    context.reader.dispose();

    return true;
}

void ParticleSystemManager::update(float deltaTime) {

    const unsigned count = mParticleSystems.size();
    for (unsigned i = 0; i < count;) {
        if (mParticleSystems[i]->update(deltaTime)) {
            mParticleSystems[i] = std::move(mParticleSystems[count]);
            mParticleSystems.pop_back();
        }
        else {
            ++i;
        }
    }
}
