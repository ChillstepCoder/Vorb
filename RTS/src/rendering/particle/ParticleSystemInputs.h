#pragma once

// MAINTAIN TYPE ORDERING
enum class ParticleSystemInputName : ui8 {
    INVALID = 0,

    UINT_END,
    UINT_TERM = UINT_END - 1,

    FLOAT_END,
    FLOAT_TERM = FLOAT_END - 1,

    VEC2_END,
    VEC2_TERM = VEC2_END - 1,

    Vec3ImpactDirection,
    Vec3ImpactSurfaceNormal,

    VEC3_END,
    VEC3_TERM = VEC3_END - 1,

    MeshSource,
    MeshTarget,

    MESH_END,
    MESH_TERM = MESH_END - 1,

    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleSystemInputName,
    ENUM_FIELD_SIMPLE(ParticleSystemInputName, INVALID),
    ENUM_FIELD_SIMPLE(ParticleSystemInputName, Vec3ImpactDirection),
    ENUM_FIELD_SIMPLE(ParticleSystemInputName, Vec3ImpactSurfaceNormal),
    ENUM_FIELD_SIMPLE(ParticleSystemInputName, MeshSource),
    ENUM_FIELD_SIMPLE(ParticleSystemInputName, MeshTarget)
);

class ModelDef;
struct ParticleSystemMeshInput {
    const ModelDef* mModelDef = nullptr;
};
YML_WRITE_DECL(ParticleSystemMeshInput);
YML_READ_DECL(ParticleSystemMeshInput);

using ParticleSystemInput = std::variant<ui32, f32, f32v2, f32v3, ParticleSystemMeshInput>;
YML_WRITE_DECL(ParticleSystemInput);
YML_READ_DECL(ParticleSystemInput);

struct ParticleSystemInputs {
    void setUIntInput(ParticleSystemInputName name, ui32 value);
    void setFloatInput(ParticleSystemInputName name, f32 value);
    void setVec2Input(ParticleSystemInputName name, f32v2 value);
    void setVec3Input(ParticleSystemInputName name, f32v3 value);
    void setMeshInput(ParticleSystemInputName name, ParticleSystemMeshInput value);
    void setInput(ParticleSystemInputName name, ParticleSystemInput input);

    void addDefaultInput(ParticleSystemInputName name);

    ui32 getUIntInput(ParticleSystemInputName name, ui32 defaultIfNotFound = 0) const;
    f32 getFloatInput(ParticleSystemInputName name, f32 defaultIfNotFound = 0.0f) const;
    f32v2 getVec2Input(ParticleSystemInputName name, f32v2 defaultIfNotFound = f32v2(0.0f)) const;
    f32v3 getVec3Input(ParticleSystemInputName name, f32v3 defaultIfNotFound = f32v3(0.0f)) const;
    ParticleSystemMeshInput getMeshInput(ParticleSystemInputName name, ParticleSystemMeshInput defaultIfNotFound = ParticleSystemMeshInput()) const;

    void readYmlNode(const ryml::ConstNodeRef& node, ParticleSystemInputName name);

    // TODO: Profile against plain vectors for small N
    FlatMap<ParticleSystemInputName, ParticleSystemInput> inputMap;
};

using ParticleSystemInputsPtr = std::shared_ptr<ParticleSystemInputs>;
using ConstParticleSystemInputsPtr = std::shared_ptr<const ParticleSystemInputs>;