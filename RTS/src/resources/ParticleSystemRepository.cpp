#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include <fstream>

#include "rendering/MaterialShaderDef.h"

#include "resources/EffectRepository.h"

#include "resources/MaterialRepository.h"

#include <imgui.h>

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

    // Lifetime
    ryml::NodeRef lifetimeNode = root.append_child() << ryml::key("lifetime");
    lifetimeNode << particleSystem.mLifetimeSec;

    // Inputs
    ryml::NodeRef inputsNode = root.append_child() << ryml::key("inputs");
    inputsNode |= ryml::SEQ;
    for (auto it = particleSystem.mDefaultInputs->inputMap.begin(); it != particleSystem.mDefaultInputs->inputMap.end(); ++it) {
        ryml::NodeRef newNode = inputsNode.append_child();
        newNode |= ryml::MAP;
        ryml::NodeRef innerNode = newNode[c4::to_csubstr(ENUM_CSTR(ParticleSystemInputName, it->first))];
        innerNode << it->second;
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
    if (particleEmitter.mMaterialRef.isValid()) {
        innerNode[EMITTER_MATERIAL_KEY] << particleEmitter.mMaterialRef;
    }
    innerNode[EMITTER_SHADER_KEY] << particleEmitter.mShaderRef;

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
    particleEmitter.mMaterialRef = getDefaultMaterialID();
    particleEmitter.mShaderRef = CStrToken("particle_3d");
    
    // Deserialize config
    yml::tryReadValue(node, EMITTER_SCALE_KEY, particleEmitter.mDefaultScale);
    yml::tryReadValue(node, EMITTER_COLOR_KEY, particleEmitter.mDefaultColor);
    yml::tryReadValue(node, EMITTER_MAX_PARTICLES_KEY, particleEmitter.mMaxParticles);
    yml::tryReadValue(node, EMITTER_LIFETIME_KEY, particleEmitter.mLifetimeSec);
    yml::tryReadValue(node, EMITTER_PARTICLE_LIFESPAN_KEY, particleEmitter.mDefaultParticleLifespanSec);
    yml::tryReadValue(node, EMITTER_LOOPING_KEY, particleEmitter.mLooping);
    yml::tryReadValue(node, EMITTER_BLEND_KEY, particleEmitter.mBlendMode);
    yml::tryReadValue(node, EMITTER_MATERIAL_KEY, particleEmitter.mMaterialRef);
    yml::tryReadValue(node, EMITTER_SHADER_KEY, particleEmitter.mShaderRef);

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

void ParticleSystemRepository::fixupLoadedAsset(AssetID assetId) {
    ParticleSystemDef& newDef = *mAssets[assetId];

    FlatSet<ParticleEmitterVariableNameUInt> uintVars;
    FlatSet<ParticleEmitterVariableNameFloat> floatVars;
    FlatSet<ParticleEmitterVariableNameVec2> vec2Vars;
    FlatSet<ParticleEmitterVariableNameVec3> vec3Vars;

    for (auto& emitter : newDef.mEmitters) {
        emitter.mUIntVariables.clear();
        emitter.mFloatVariables.clear();
        emitter.mVec2Variables.clear();
        emitter.mVec3Variables.clear();

        uintVars.clear();
        floatVars.clear();
        vec2Vars.clear();
        vec3Vars.clear();
        emitter.mActiveComponents.clearBits();

        // Track all needed variables and components
        RequiredEmitterVariables reqVars{ uintVars, floatVars, vec2Vars, vec3Vars };

        // Add required variables, validate modules, and add required components
        for (auto moduleVector : { &emitter.mModules.mEmitterUpdate, &emitter.mModules.mParticleInit, &emitter.mModules.mParticleUpdate }) {
            int i = 0;
            for (auto& module : *moduleVector) {
                module->addRequiredVariables(reqVars);

                std::span<const std::unique_ptr<CPUParticleEmitterModule>> modulesAbove(moduleVector->data(), i);
                if (!module->validatePrerequesiteModules(modulesAbove)) {
                    module->setIsValid(false);
                }

                emitter.mActiveComponents |= module->getRequiredComponents();
                ++i;
            }
        }

        // Copy the variables
        emitter.mUIntVariables.insert(emitter.mUIntVariables.end(), uintVars.begin(), uintVars.end());
        emitter.mFloatVariables.insert(emitter.mFloatVariables.end(), floatVars.begin(), floatVars.end());
        emitter.mVec2Variables.insert(emitter.mVec2Variables.end(), vec2Vars.begin(), vec2Vars.end());
        emitter.mVec3Variables.insert(emitter.mVec3Variables.end(), vec3Vars.begin(), vec3Vars.end());

        // Validate params after we have full state
        for (auto moduleVector : { &emitter.mModules.mEmitterUpdate, &emitter.mModules.mParticleInit, &emitter.mModules.mParticleUpdate }) {
            for (auto& module : *moduleVector) {
                if (!module->validateParams(emitter)) {
                    module->setIsValid(false);
                }
            }
        }

        assert(emitter.mShaderRef.isLoaded());

        // Add shader bindings
        emitter.mShaderBindings.clear();
        const MaterialShaderDef& shaderDef = emitter.mShaderRef.getLoadedAsset<MaterialShaderDef>();
        const vg::GLProgram::SsboMap& bindings = shaderDef.mProgram.getSsboBindings();
        for (const auto& [name, index] : bindings) {
            ParticleVariableNameVariant variant;
            if (tryReadAnyEnumFromTupleIntoVariant<ParticleEmitterVariableEnums>(name, variant)) {
                emitter.mShaderBindings.emplace_back(variant, index);
            }
        }
    }
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

            assert(newEmitter.mShaderRef.isValid());
            newDef.addDependency(newEmitter.mShaderRef.getAssetHandleBase());
        }

        if (tree.rootref().has_child("lifetime")) {
            tree.rootref()["lifetime"] >> newDef.mLifetimeSec;
        }

        if (tree.rootref().has_child("inputs")) {
            ryml::ConstNodeRef inputsNode = tree.rootref()["inputs"];
            for (ryml::ConstNodeRef seqNode : inputsNode.children()) {

                ryml::ConstNodeRef innerNode = seqNode.first_child();
                const auto& enumMap = getGlobalEnumNameMap<ParticleSystemInputName>();
                for (auto& [key, value] : enumMap) {
                    const c4::csubstr nodeKey = innerNode.key();
                    const c4::csubstr vstr = c4::csubstr(value.data(), value.length());
                    if (nodeKey == vstr) {
                        newDef.mDefaultInputs->readYmlNode(innerNode, key);
                        break;
                    }
                }
            }
        }

        if (newDef.getDependencies()->areAllAssetsLoaded()) {
            fixupLoadedAsset(assetID);
            return true;
        }
        else {
            // Make sure we load all dependencies (Shaders)
            assetLoader.requestAssetLoadWithDependencies(nullptr, [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
                fixupLoadedAsset(assetID);
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
