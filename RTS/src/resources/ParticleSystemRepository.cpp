#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include <fstream>
#include <Vorb/io/IOManager.h>

#include "resources/EffectRepository.h"
#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "rendering/MaterialShaderRepository.h"

#include <imgui.h>
#include <imgui_internal.h>

const vio::Path PARTICLE_SYSTEM_PATH = "data/particle";

// Yml keys
constexpr const char* const EMITTER_SCALE_KEY("scale");
constexpr const char* const EMITTER_COLOR_KEY("color");
constexpr const char* const EMITTER_MAX_PARTICLES_KEY("max_particles");
constexpr const char* const EMITTER_LIFETIME_KEY("lifetime");
constexpr const char* const EMITTER_PARTICLE_LIFESPAN_KEY("p_lifespan");
constexpr const char* const EMITTER_LOOPING_KEY("looping");
constexpr const char* const EMITTER_BLEND_KEY("blend");
constexpr const char* const EMITTER_MATERIAL_KEY("mat");
constexpr const char* const EMITTER_SHADER_KEY("shader");

void ParticleSystemRepository::setDefaultMaterialID(MaterialID id)
{
    mDefaultMaterial = id;
    mDefaultMaterialHandle = MaterialRepository::get().getAssetHandle(id);
}

bool ParticleSystemRepository::saveAsset(AssetID id) {
    const ParticleSystemDef& particleSystem = *mAssets[id];

    mEmitterStrBuf.clear();

    vio::Path filePath = getAssetFilePath(id);
    // TODO: DIALOG
    if (filePath.isNull()) {
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

bool ParticleSystemRepository::renderImguiAssetActions(AssetMetadata& asset) {
    bool shouldRefresh = false;
    if (ImGui::BeginMenu("Asset Actions")) {
        if (ImGui::MenuItem("Create EffectDef")) {
            EffectRepository& effectRepo = EffectRepository::get();
            AssetHandlePtr<EffectDef> newAssetHandle = effectRepo.editorTryAddNewAsset(asset.mName);
            if (newAssetHandle) {
                EffectDef* newAssetPtr = newAssetHandle->editorTryGetMutableAsset();
                assert(newAssetPtr);
                newAssetPtr->mParticleSystemName = asset.mName;
                newAssetPtr->addDependency(getAssetHandle(asset.getId()));

                vio::Path targetDir = getAssetFilePath(asset.getId()).trimEnd();
                targetDir /= (asset.mName.toString() + "." + effectRepo.getAssetExtension().toString());

                effectRepo.changeAssetFilePath(newAssetHandle->getAssetID(), targetDir);
                effectRepo.saveAsset(newAssetHandle->getAssetID());
                shouldRefresh = true;
            }
            else {
                showMessage("Could not create asset. There is probably already one with the same name.");
            }
        }
        ImGui::EndMenu();
    }
    return shouldRefresh;
}

void ParticleSystemRepository::saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter) {
    // We must cache the strings since their data will be referenced when we are finished saving
    // due to c4::to_csubstr using a pointer to the string data
    const nString& nameStr = mEmitterStrBuf.emplace_back(particleEmitter.mEmitterName.toString());
    ryml::NodeRef innerNode = node[c4::to_csubstr(nameStr)];
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
    if (particleEmitter.mDefaultMaterialName.isValid()) {
        innerNode[EMITTER_MATERIAL_KEY] << particleEmitter.mDefaultMaterialName;
    }
    innerNode[EMITTER_SHADER_KEY] << particleEmitter.mShaderName;

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

    // Defaults
    particleEmitter.mDefaultMaterialID = getDefaultMaterialID();
    particleEmitter.mShaderName = CStrToken("particle_bb_3d");
    
    // Deserialize config
    yml::tryReadValue(node, EMITTER_SCALE_KEY, particleEmitter.mDefaultScale);
    yml::tryReadValue(node, EMITTER_COLOR_KEY, particleEmitter.mDefaultColor);
    yml::tryReadValue(node, EMITTER_MAX_PARTICLES_KEY, particleEmitter.mMaxParticles);
    yml::tryReadValue(node, EMITTER_LIFETIME_KEY, particleEmitter.mLifetimeSec);
    yml::tryReadValue(node, EMITTER_PARTICLE_LIFESPAN_KEY, particleEmitter.mDefaultParticleLifespanSec);
    yml::tryReadValue(node, EMITTER_LOOPING_KEY, particleEmitter.mLooping);
    yml::tryReadValue(node, EMITTER_BLEND_KEY, particleEmitter.mBlendMode);
    if (yml::tryReadValue(node, EMITTER_MATERIAL_KEY, particleEmitter.mDefaultMaterialName)) {
        particleEmitter.mDefaultMaterialID = MaterialRepository::get().getAssetID(particleEmitter.mDefaultMaterialName);
    }
    yml::tryReadValue(node, EMITTER_SHADER_KEY, particleEmitter.mShaderName);

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
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        nString fileData = readFileToString(filePath);

        ryml::Tree tree = YmlSerializer::parseFileData(fileData);

        ParticleSystemDef& newDef = *static_cast<ParticleSystemDef*>(assetDataPtr);

        // Loop through emitters
        ryml::ConstNodeRef emittersNode = tree.rootref()["emitters"];
        for (ryml::ConstNodeRef seqNode : emittersNode.children()) {
            ryml::ConstNodeRef innerNode = seqNode.first_child();
            ParticleEmitterDef& newEmitter = newDef.mEmitters.emplace_back();
            newEmitter.mEmitterName = StrToken(innerNode.key().data(), innerNode.key().size());
            if (!loadParticleEmitter(innerNode, newEmitter)) {
                panic("Failed to load particle emitter {} {}", filePath.getFileNameNoExtension(), filePath.getString());
            }
            assert(newEmitter.mShaderName.isValid());
            newDef.addDependency(MaterialShaderRepository::get().getAssetHandle(newEmitter.mShaderName));
        }

        if (newDef.getDependencies()->areAllAssetsLoaded()) {
            return true;
        }
        else {
            // Make sure we load all dependencies (Shaders)
            assetLoader.requestAssetLoadWithDependencies(nullptr, [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
                return true;
            }, assetID,
                assetDataPtr,
                filePath,
                mLoadedAssets[assetID].get(),
                nullptr,
                newDef.getDependencies()
            );
        }

        return false;
    };
}
