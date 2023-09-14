#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include <fstream>
#include <Vorb/io/IOManager.h>

#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"

const vio::Path PARTICLE_SYSTEM_PATH = "data/particle";

// Yml keys
constexpr const char* const EMITTER_SCALE_KEY("scale");
constexpr const char* const EMITTER_COLOR_KEY("color");
constexpr const char* const EMITTER_MAX_PARTICLES_KEY("max_particles");
constexpr const char* const EMITTER_LIFETIME_KEY("lifetime");
constexpr const char* const EMITTER_PARTICLE_LIFESPAN_KEY("p_lifespan");
constexpr const char* const EMITTER_LOOPING_KEY("looping");
constexpr const char* const EMITTER_BLEND_KEY("blend");

bool ParticleSystemRepository::saveAsset(AssetID id) {
    const ParticleSystemDef& particleSystem = *mAssets[id];

    vio::Path filePath = getAssetFilePath(id);
    // TODO: DIALOG
    if (!filePath.isValid()) {
        changeAssetFilePath(id, PARTICLE_SYSTEM_PATH / particleSystem.getName().toString() + vio::Path(".psys"));
    }

    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;

    // Emitters
    ryml::NodeRef emittersNode = root.append_child() << ryml::key("emitters");
    emittersNode |= ryml::SEQ;

    for (auto&& emitter : particleSystem.mEmitters) {
        ryml::NodeRef newNode = emittersNode.append_child();
        newNode |= ryml::MAP;
        saveParticleEmitter(newNode, emitter);
    }

    std::stringstream ss;
    ss << tree;
    nString str = ss.str();
    return saveAssetContents(particleSystem, filePath, str.c_str(), str.size());
}

void ParticleSystemRepository::saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter) {
    ryml::NodeRef innerNode = node[c4::to_csubstr(particleEmitter.mEmitterName)];
    innerNode |= ryml::MAP;

    // Serialize config
    innerNode[EMITTER_SCALE_KEY] << particleEmitter.mDefaultScale;
    innerNode[EMITTER_COLOR_KEY] << particleEmitter.mDefaultColor;
    innerNode[EMITTER_MAX_PARTICLES_KEY] << particleEmitter.mMaxParticles;
    //innerNode["default_mat"] << particleEmitter.; // NEEDS STRING
    innerNode[EMITTER_LIFETIME_KEY] << particleEmitter.mLifetimeSec;
    innerNode[EMITTER_PARTICLE_LIFESPAN_KEY] << particleEmitter.mDefaultParticleLifespanSec;
    innerNode[EMITTER_LOOPING_KEY] << particleEmitter.mLooping;
    innerNode[EMITTER_BLEND_KEY] << particleEmitter.mBlendMode;
    { // Emitter Update
        ryml::NodeRef updateNode = innerNode["e_update"];
        updateNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mModules.mEmitterUpdate) {
            ryml::NodeRef innerNode = updateNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }

    { // Particle Init
        ryml::NodeRef initNode = innerNode["p_init"];
        initNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mModules.mParticleInit) {
            ryml::NodeRef innerNode = initNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }

    { // Particle Update
        ryml::NodeRef updateNode = innerNode["p_update"];
        updateNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mModules.mParticleUpdate) {
            ryml::NodeRef innerNode = updateNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }
}

bool ParticleSystemRepository::loadParticleEmitter(ryml::ConstNodeRef node, ParticleEmitterDef& particleEmitter) {

    // TODO: Material and shader
    particleEmitter.mDefaultMaterialID = getDefaultMaterialID();
    particleEmitter.mShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("textured_particle_3d_bb");
    
    // Deserialize config
    yml::tryReadValue(node, EMITTER_SCALE_KEY, particleEmitter.mDefaultScale);
    yml::tryReadValue(node, EMITTER_COLOR_KEY, particleEmitter.mDefaultColor);
    yml::tryReadValue(node, EMITTER_MAX_PARTICLES_KEY, particleEmitter.mMaxParticles);
    yml::tryReadValue(node, EMITTER_LIFETIME_KEY, particleEmitter.mLifetimeSec);
    yml::tryReadValue(node, EMITTER_PARTICLE_LIFESPAN_KEY, particleEmitter.mDefaultParticleLifespanSec);
    yml::tryReadValue(node, EMITTER_LOOPING_KEY, particleEmitter.mLooping);
    yml::tryReadValue(node, EMITTER_BLEND_KEY, particleEmitter.mBlendMode);

    { // Emitter Update
        ryml::ConstNodeRef updateNode = node["e_update"];
        if (updateNode.is_seq()) {
            for (ryml::ConstNodeRef seqNode : updateNode.children()) {
                ryml::ConstNodeRef innerNode = seqNode.first_child();
                CPUParticleEmitterModule& newModule = *particleEmitter.mModules.mEmitterUpdate.emplace_back(yml::cloneYmlObject<CPUParticleEmitterModule>(innerNode.key()));
                if (!newModule.loadFromYml(innerNode)) return false;
            }
        }
    }

    { // Particle Init
        ryml::ConstNodeRef initNode = node["p_init"];
        if (initNode.is_seq()) {
            for (ryml::ConstNodeRef seqNode : initNode.children()) {
                ryml::ConstNodeRef innerNode = seqNode.first_child();
                CPUParticleEmitterModule& newModule = *particleEmitter.mModules.mParticleInit.emplace_back(yml::cloneYmlObject<CPUParticleEmitterModule>(innerNode.key()));
                if (!newModule.loadFromYml(innerNode)) return false;
            }
        }
    }

    { // Particle Update
        ryml::ConstNodeRef updateNode = node["p_update"];
        if (updateNode.is_seq()) {
            for (ryml::ConstNodeRef seqNode : updateNode.children()) {
                ryml::ConstNodeRef innerNode = seqNode.first_child();
                CPUParticleEmitterModule& newModule = *particleEmitter.mModules.mParticleUpdate.emplace_back(yml::cloneYmlObject<CPUParticleEmitterModule>(innerNode.key()));
                if (!newModule.loadFromYml(innerNode)) return false;
            }
        }
    }
    return true;
}

AssetLoadFunc ParticleSystemRepository::getAssetLoadFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        nString fileData = readFileToString(filePath);

        ryml::Tree tree = YmlSerializer::parseFileData(fileData);

        ParticleSystemDef* newDef = static_cast<ParticleSystemDef*>(assetDataPtr);
        if (!newDef) {
            LOG_CRITICAL("Failed to load {} from {} - already exists", filePath.getFileNameNoExtension(), filePath.getString());
            pError("Failed to load " + filePath.getString() + " already exists");
            return;
        }

        // Loop through emitters
        ryml::ConstNodeRef emittersNode = tree.rootref()["emitters"];
        for (ryml::ConstNodeRef seqNode : emittersNode.children()) {
            ryml::ConstNodeRef innerNode = seqNode.first_child();
            ParticleEmitterDef& newEmitter = newDef->mEmitters.emplace_back();
            newEmitter.mEmitterName = nString(std::string_view(innerNode.key().data(), innerNode.key().size()));
            if (!loadParticleEmitter(innerNode, newEmitter)) {
                assert(false);
                return;
            }
        }
    };
}
