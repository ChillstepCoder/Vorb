#include "stdafx.h"
#include "ParticleSystemRepository.h"

#include <fstream>
#include <Vorb/io/IOManager.h>

#include "rendering/particle/BuiltinCPUParticleEmitterModules.h"

const vio::Path PARTICLE_SYSTEM_PATH = "data/particle";

SERIALIZABLE_SIMPLE(ParticleSystemDef,
    o.mSystemName, "name"sv
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

    for (const ryml::ConstNodeRef n : tree.rootref().children()) {
        assert(false); // TODO:
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
    ryml::NodeRef child = root.append_child() << ryml::key("test2");
    child |= ryml::MAP;
    child.append_child() << ryml::key("test3") << "GOODBYE";
    child.append_child() << ryml::key("test4") << "WORLD";
    root["pi"] << ryml::fmt::real(3.141592654, 5);
    root["xmas"] << ryml::fmt::boolalpha(true);
    root["thiswork"];

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
    return true;
}

void ParticleSystemRepository::saveParticleEmitter(keg::YAMLWriter& writer, const ParticleEmitterDef& particleEmitter)
{
    YmlSerializable::beginMap(writer);

    YmlSerializable::pushKeyValue(writer, "e_update");
    YmlSerializable::beginSequence(writer);
    for (auto& module : particleEmitter.mEmitterUpdateModules) {
        module->saveYml(writer);
    }
    YmlSerializable::endSequence(writer);
    YmlSerializable::pushKeyValue(writer, "p_init");
    YmlSerializable::beginSequence(writer);
    for (auto& module : particleEmitter.mParticleInitModules) {
        module->saveYml(writer);
    }
    YmlSerializable::endSequence(writer);
    YmlSerializable::pushKeyValue(writer, "p_update");
    YmlSerializable::beginSequence(writer);
    for (auto& module : particleEmitter.mParticleUpdateModules) {
        module->saveYml(writer);
    }
    YmlSerializable::endSequence(writer);
    YmlSerializable::endMap(writer);
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
