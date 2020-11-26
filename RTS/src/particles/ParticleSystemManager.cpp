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

ParticleSystem* ParticleSystemManager::createParticleSystem(const f32v3& position, const f32v3& direction, const nString& name) {

    auto&& it = mParticleSystemData.find(name);
    assert(it != mParticleSystemData.end());
    ParticleSystemData& systemData = it->second;

    // TODO: Reduce to 1 map lookup
    bool isNew = (mParticleSystems.find(name) == mParticleSystems.end());

    ParticleSystemArray& array = mParticleSystems[name];

    // Insert into sort array
    if (isNew) {
        bool shouldAppend = true;
        for (auto&& it = mSystemLayerSortOrder.begin(); it != mSystemLayerSortOrder.end(); ++it) {
            if (systemData.layer <= it->first) {
                mSystemLayerSortOrder.insert(it, std::pair<ui32, nString>(systemData.layer, name));
                shouldAppend = false;
                break;
            }
        }
        if (shouldAppend) {
            mSystemLayerSortOrder.emplace_back(systemData.layer, name);
        }
    }

    array.emplace_back(std::make_unique<ParticleSystem>(position, direction, systemData));
    return array.back().get();
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

void ParticleSystemManager::update(float deltaTime, const f32v2& playerPos) {

    for (auto&& it : mParticleSystems) {
        ParticleSystemArray& systemArray = it.second;
        for (unsigned i = 0; i < systemArray.size();) {
            if (systemArray[i]->update(deltaTime, playerPos)) {
                if (i != systemArray.size() - 1) {
                    systemArray[i] = std::move(systemArray.back());
                }
                systemArray.pop_back();
            }
            else {
                ++i;
            }
        }
    }
}
