#include "stdafx.h"
#include "ParticleSystemInputs.h"

#include "definitions/ModelDef.h"

YML_WRITE_DEF(ParticleSystemMeshInput) {
    ModelAssetRef ref = o.mModelDef ? ModelAssetRef(o.mModelDef->getID()) : ModelAssetRef();
    n->operator<<(ref);
}
YML_READ_DEF(ParticleSystemMeshInput) {
    ModelAssetRef ref;
    n.operator>>(ref);
    if (ref.isValid()) {
        target->mModelDef = &ref.getLoadedOrUnloadedAsset<ModelDef>();
    }
    else {
        target->mModelDef = nullptr;
    }
    return true;
}

YML_WRITE_DEF(ParticleSystemInput) {
    std::visit([&](auto&& arg) {
        n->operator<<(arg);
    }, o);
}
YML_READ_DEF(ParticleSystemInput) {
    std::visit([&](auto&& arg) {
        n.operator>>(arg);
    }, *target);
    return true;
}

void ParticleSystemInputs::setInput(ParticleSystemInputName name, ParticleSystemInput input) {
    if (name <= ParticleSystemInputName::UINT_TERM) {
        assert(std::holds_alternative<ui32>(input));
    }
    else if (name <= ParticleSystemInputName::FLOAT_TERM) {
        assert(std::holds_alternative<f32>(input));
    }
    else if (name <= ParticleSystemInputName::VEC2_TERM) {
        assert(std::holds_alternative<f32v2>(input));
    }
    else if (name <= ParticleSystemInputName::VEC3_TERM) {
        assert(std::holds_alternative<f32v3>(input));
    }
    else if (name <= ParticleSystemInputName::MESH_TERM) {
        assert(std::holds_alternative<ParticleSystemMeshInput>(input));
    }
    else assert(false);


    inputMap[name] = input;
}

void ParticleSystemInputs::setUIntInput(ParticleSystemInputName name, ui32 value) {
    assert(name <= ParticleSystemInputName::UINT_TERM);
    inputMap[name] = value;
}

ParticleSystemMeshInput ParticleSystemInputs::getMeshInput(ParticleSystemInputName name, ParticleSystemMeshInput defaultIfNotFound /*= ParticleSystemMeshInput()*/) const {
    assert(name > ParticleSystemInputName::VEC3_TERM && name <= ParticleSystemInputName::MESH_TERM);
    auto it = inputMap.find(name);
    if (it != inputMap.end()) [[likely]] return std::get<ParticleSystemMeshInput>(it->second);
    return defaultIfNotFound;
}

f32 ParticleSystemInputs::getFloatInput(ParticleSystemInputName name, f32 defaultIfNotFound /*= 0.0f*/) const {
    assert(name > ParticleSystemInputName::UINT_TERM && name <= ParticleSystemInputName::FLOAT_TERM);
    auto it = inputMap.find(name);
    if (it != inputMap.end()) [[likely]] return std::get<f32>(it->second);
    return defaultIfNotFound;
}

f32v3 ParticleSystemInputs::getVec3Input(ParticleSystemInputName name, f32v3 defaultIfNotFound /*= f32v3(0.0f)*/) const {
    assert(name > ParticleSystemInputName::VEC2_TERM && name <= ParticleSystemInputName::VEC3_TERM);
    auto it = inputMap.find(name);
    if (it != inputMap.end()) [[likely]] return std::get<f32v3>(it->second);
    return defaultIfNotFound;
}

ui32 ParticleSystemInputs::getUIntInput(ParticleSystemInputName name, ui32 defaultIfNotFound /*= 0*/) const {
    assert(name <= ParticleSystemInputName::UINT_TERM);
    auto it = inputMap.find(name);
    if (it != inputMap.end()) [[likely]] return std::get<ui32>(it->second);
    return defaultIfNotFound;
}

void ParticleSystemInputs::setVec3Input(ParticleSystemInputName name, f32v3 value) {
    assert(name > ParticleSystemInputName::VEC2_TERM && name <= ParticleSystemInputName::VEC3_TERM);
    inputMap[name] = value;
}

void ParticleSystemInputs::setMeshInput(ParticleSystemInputName name, ParticleSystemMeshInput value) {
    assert(name > ParticleSystemInputName::VEC3_TERM && name <= ParticleSystemInputName::MESH_TERM);
    inputMap[name] = value;
}

f32v2 ParticleSystemInputs::getVec2Input(ParticleSystemInputName name, f32v2 defaultIfNotFound /*= f32v2(0.0f)*/) const {
    assert(name > ParticleSystemInputName::FLOAT_TERM && name <= ParticleSystemInputName::VEC2_TERM);
    auto it = inputMap.find(name);
    if (it != inputMap.end()) [[likely]] return std::get<f32v2>(it->second);
    return defaultIfNotFound;
}

void ParticleSystemInputs::addDefaultInput(ParticleSystemInputName name) {
    if (name <= ParticleSystemInputName::UINT_TERM) setUIntInput(name, 0);
    else if (name <= ParticleSystemInputName::FLOAT_TERM) setFloatInput(name, 0.0f);
    else if (name <= ParticleSystemInputName::VEC2_TERM) setVec2Input(name, f32v2(0.0f));
    else if (name <= ParticleSystemInputName::VEC3_TERM) setVec3Input(name, f32v3(0.0f));
    else if (name <= ParticleSystemInputName::MESH_TERM) setMeshInput(name, ParticleSystemMeshInput());
    else assert(false);
}

void ParticleSystemInputs::setFloatInput(ParticleSystemInputName name, f32 value) {
    assert(name > ParticleSystemInputName::UINT_TERM && name <= ParticleSystemInputName::FLOAT_TERM);
    inputMap[name] = value;
}

void ParticleSystemInputs::setVec2Input(ParticleSystemInputName name, f32v2 value) {
    assert(name > ParticleSystemInputName::FLOAT_TERM && name <= ParticleSystemInputName::VEC2_TERM);
    inputMap[name] = value;
}

void ParticleSystemInputs::readYmlNode(const ryml::ConstNodeRef& node, ParticleSystemInputName name) {
    ParticleSystemInput input;
    if (name <= ParticleSystemInputName::UINT_TERM) {
        ui32 v;
        node >> v;
        input = v;
    }
    else if (name <= ParticleSystemInputName::FLOAT_TERM) {
        f32 v;
        node >> v;
        input = v;
    }
    else if (name <= ParticleSystemInputName::VEC2_TERM) {
        f32v2 v;
        node >> v;
        input = v;
    }
    else if (name <= ParticleSystemInputName::VEC3_TERM) {
        f32v3 v;
        node >> v;
        input = v;
    }
    else if (name <= ParticleSystemInputName::MESH_TERM) {
        ParticleSystemMeshInput v;
        node >> v;
        input = v;
    }
    else {
        assert(false);
    }

    inputMap[name] = input;
}
