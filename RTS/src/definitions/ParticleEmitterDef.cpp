#include "stdafx.h"
#include "ParticleEmitterDef.h"

YML_WRITE_DEF(ParticleVariableNameVariant) {
    std::visit([&](auto&& arg) {
        n->operator<<(arg);
    }, o);
}
YML_READ_DEF(ParticleVariableNameVariant) {
    nString name;
    n.operator>>(name);
    std::string_view str = name;
    ParticleVariableNameVariant var;

    if (tryReadAnyEnumFromTupleIntoVariant<ParticleEmitterVariableEnums>(str, var)) {
        return true;
    }

    return false;
}

SERIALIZABLE_SIMPLE(ParticleEmitterDef::ShaderBinding,
    make_field(o.mVariableName, "name"),
    make_field(o.mShaderBindingIndex, "index")
)

