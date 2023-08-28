#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include <fstream>
#include <Vorb/io/IOManager.h>

#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

const vio::Path PARTICLE_SYSTEM_PATH = "data/particle";

SERIALIZABLE_SIMPLE(ParticleSystemDef,
    make_field(o.mSystemName, "name"sv)
)

ParticleSystemRepository::ParticleSystemRepository(vio::IOManager& ioManager, MaterialRepository& materialRepo) :
    IAssetRepository(ioManager),
    mMaterialRepository(materialRepo) {

    mEmitterModules.reserve(e_count(BuiltinCPUParticleEditorModules));

    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnBurst));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SpawnRate));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::RingBurst));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::ConeBurst));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetPosition));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetVelocity));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetColor));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetScale));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::SetPositionFromShape));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::ApplyForce));
    mEmitterModules.emplace_back(createCPUParticleEmitterModule(BuiltinCPUParticleEditorModules::DragForce));
    static_assert(e_count(BuiltinCPUParticleEditorModules) == 11);

    mEmitterOperations.reserve(25);

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddFloatToVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyFloatToVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_AddFloat>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_MultiplyFloat>());

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetColor>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec4>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetVec2>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetFloat>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_SetUInt>());

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_NegateVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_NegateVec2>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_NegateFloat>());

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_ConvertFloatToVec4>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_ConvertFloatToVec3>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_ConvertFloatToVec2>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_ConvertFloatToUInt>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_ConvertUIntToFloat>());

    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_QueryPosition>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_QueryVelocity>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_QueryScale>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_QueryRotation>());
    mEmitterOperations.emplace_back(std::make_unique<CPUPEO_QueryNormalizedLifetime>());

    mEmitterOperations.shrink_to_fit();

    // Init lookups for serialization
    for (auto&& module : mEmitterModules) {
        mModulesYmlLookup[module->getYmlName()] = module.get();
    }
    for (auto&& operation : mEmitterOperations) {
        mOperationsYmlLookup[operation->getYmlName()] = operation.get();
    }

    static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);
}

ParticleSystemRepository::~ParticleSystemRepository() {

}

void ParticleSystemRepository::loadParticleSystemFile(const vio::Path& filePath)
{
    nString data;
    mIoManager.readFileToString(filePath.getCString(), data);
    ryml::Tree tree = YmlSerializer::parseFileData(data);

    ParticleSystemDef* newDef = tryAddNewParticleSystem(filePath.getFileNameNoExtension());
    if (!newDef) {
        LOG_CRITICAL("Failed to load {} from {} already exists", filePath.getFileNameNoExtension(), filePath.getString());
        pError("Failed to load " + filePath.getString() + " already exists");
        return;
    }

    // Loop through emitters
    for (const ryml::ConstNodeRef n : tree.rootref().children()) {
        
    }
}

bool ParticleSystemRepository::saveParticleSystem(const ParticleSystemDef& particleSystem) {
    // TODO: DIALOG
    if (!particleSystem.getDiskLocation().isValid()) {
        particleSystem.setDiskLocation(PARTICLE_SYSTEM_PATH / particleSystem.mSystemName + vio::Path(".psys"));
        // TODO: Save unshared emitters near this one
        /*for (auto&& emitter : particleSystem.mEmitters) {

        }*/
    }

    
    //tree.root_id();
    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;
    ryml::NodeRef emittersNode = root.append_child() << ryml::key("emitters");
    emittersNode |= ryml::SEQ;

    for (auto&& emitter : particleSystem.mEmitters) {
        ryml::NodeRef newNode = emittersNode.append_child();
        newNode |= ryml::MAP;
        saveParticleEmitter(newNode, emitter);
    }

    /* ryml::NodeRef child = root.append_child() << ryml::key("test2");
     child |= ryml::MAP;
     child.append_child() << ryml::key("test3") << "GOODBYE";
     child.append_child() << ryml::key("test4") << "WORLD";
     root["pi"] << ryml::fmt::real(3.141592654, 5);
     root["xmas"] << ryml::fmt::boolalpha(true);
     root["thiswork"];*/

    // OLD
    /*keg::YAMLWriter writer;
    YmlSerializable::beginMap(writer);
    YmlSerializable::pushKeyValue(writer, "emitters");
    YmlSerializable::beginSequence(writer);

    for (auto&& emitter : particleSystem.mEmitters) {
        YmlSerializable::beginMap(writer);
        YmlSerializable::pushKeyValue(writer, emitter.mEmitterName.c_str());
        saveParticleEmitter(writer, emitter);
        YmlSerializable::endMap(writer);
    }

    YmlSerializable::endSequence(writer);
    YmlSerializable::endMap(writer);

    return saveAssetContents(particleSystem, writer.c_str(), writer.size());
    */
    std::stringstream ss;
    ss << tree;
    nString str = ss.str();
    return saveAssetContents(particleSystem, str.c_str(), str.size());
}

void ParticleSystemRepository::saveParticleEmitter(ryml::NodeRef& node, const ParticleEmitterDef& particleEmitter) {
    ryml::NodeRef innerNode = node[c4::to_csubstr(particleEmitter.mEmitterName)];
    innerNode |= ryml::MAP;

    { // Emitter Update
        ryml::NodeRef updateNode = innerNode["e_update"];
        updateNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mEmitterUpdateModules) {
            ryml::NodeRef innerNode = updateNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }

    { // Particle Init
        ryml::NodeRef initNode = innerNode["p_init"];
        initNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mParticleInitModules) {
            ryml::NodeRef innerNode = initNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }

    { // Particle Update
        ryml::NodeRef updateNode = innerNode["p_update"];
        updateNode |= ryml::SEQ;
        for (auto& module : particleEmitter.mParticleUpdateModules) {
            ryml::NodeRef innerNode = updateNode.append_child();
            innerNode |= ryml::MAP;
            module->saveYml(innerNode);
        }
    }
}


const ParticleSystemDef& ParticleSystemRepository::getParticleSystem(const nString& itemName) const {
    auto&& it = mParticleSystemLookup.find(itemName);
    assert(it != mParticleSystemLookup.end());
    return mParticleSystems[it->second];
}

ParticleSystemDef* ParticleSystemRepository::tryAddNewParticleSystem(const nString& name) {
    if (mParticleSystemLookup.find(name) != mParticleSystemLookup.end()) {
        return nullptr;
    }

    ParticleSystemID id = mParticleSystems.size();
    mParticleSystemLookup[name] = id;
    ParticleSystemDef& newDef = mParticleSystems.emplace_back();
    newDef.mSystemName = name;
    newDef.mID = id;
    return &newDef;
}
