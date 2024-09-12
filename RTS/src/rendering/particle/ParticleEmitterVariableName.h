#pragma once


enum class ParticleEmitterVariableNameUInt : ui8 {
    INVALID,
    StartMaterial,
    EndMaterial,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameUInt,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameUInt, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameUInt, StartMaterial),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameUInt, EndMaterial)
);
static_assert(e_count(ParticleEmitterVariableNameUInt) == 3);

enum class ParticleEmitterVariableNameFloat : ui8 {
    INVALID,
    MeshSourceScale,
    MeshTargetScale,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameFloat,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameFloat, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameFloat, MeshSourceScale),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameFloat, MeshTargetScale),
);
static_assert(e_count(ParticleEmitterVariableNameFloat) == 3);

enum class ParticleEmitterVariableNameVec2 : ui8 {
    INVALID,
    MeshSourceUV,
    MeshTargetUV,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameVec2,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, MeshSourceUV),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, MeshTargetUV)
);
static_assert(e_count(ParticleEmitterVariableNameVec2) == 3);

enum class ParticleEmitterVariableNameVec3 : ui8 {
    INVALID,
    MeshSourcePos,
    MeshTargetPos,
    MeshSourceNormal,
    MeshTargetNormal,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameVec3,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, MeshSourcePos),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, MeshTargetPos),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, MeshSourceNormal),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, MeshTargetNormal)
);
static_assert(e_count(ParticleEmitterVariableNameVec3) == 5);