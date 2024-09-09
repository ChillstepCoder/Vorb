#pragma once


enum class ParticleEmitterVariableNameUInt : ui8 {
    INVALID,
    EndMaterial,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameUInt,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameUInt, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameUInt, EndMaterial)
);

enum class ParticleEmitterVariableNameFloat : ui8 {
    INVALID,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameFloat,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameFloat, INVALID)
);

enum class ParticleEmitterVariableNameVec2 : ui8 {
    INVALID,
    StartUV,
    EndUV,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameVec2,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, StartUV),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec2, EndUV)
);

enum class ParticleEmitterVariableNameVec3 : ui8 {
    INVALID,
    StartPosition,
    EndPosition,
    StartNormal,
    EndNormal,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleEmitterVariableNameVec3,
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, INVALID),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, StartPosition),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, EndPosition),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, StartNormal),
    ENUM_FIELD_SIMPLE(ParticleEmitterVariableNameVec3, EndNormal)
);