#pragma once

class CPUParticleEmitterOperation;
class CpuParticleEmitter;

#include "rendering/particle/ParticleEnumTypes.h"

#include "serialization/YmlSerializable.h"

enum class CPUparticleEmitterVariableVariantType : ui8{
    None,
    color4,
    f32v4,
    f32v3,
    f32v2,
    f32,
    ui32,
    COUNT
};
typedef std::variant<color4, f32v4, f32v3, f32v2, f32, ui32> CPUParticleEmitterVariantData;
static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);

typedef std::pair<CPUparticleEmitterVariableVariantType, CPUparticleEmitterVariableVariantType> CPUParticleEmitterVariableVariantTypePair;

class CPUParticleEmitterVariable {
public:
    CPUParticleEmitterVariable() = default;
    CPUParticleEmitterVariable(CPUParticleEmitterVariantData data) : mVarData(data) {}
    CPUParticleEmitterVariable(const CPUParticleEmitterVariable& other);

    void evaluate(CpuParticleEmitter& emitter, ParticleID id);
    bool updateAndRenderTweaker(const char*const label);
    bool loadFromYml(ryml::ConstNodeRef node, std::string_view name);
    void saveYmlData(ryml::NodeRef node, std::string_view name) const;

    // TODO: pool allocate?
    std::unique_ptr<CPUParticleEmitterOperation> mOperation = nullptr;
    CPUParticleEmitterVariantData mVarData;
};
YML_WRITE_DEF(CPUParticleEmitterVariantData) {
    std::visit([&](auto&& arg) {
        n->operator<<(arg);
    }, o);
}
YML_READ_DEF(CPUParticleEmitterVariantData) {
    std::visit([&](auto&& arg) {
        n.operator>>(arg);
    }, *target);
    return true;
}


// TODO: test perf vs virtual func
//typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation : public YmlSerializable {
public:
    CPUParticleEmitterOperation() = default;
    CPUParticleEmitterOperation(CPUParticleEmitterVariable p0, CPUParticleEmitterVariable p1) : mParam0(p0.mVarData), mParam1(p1.mVarData) {}
    CPUParticleEmitterOperation(const CPUParticleEmitterOperation* other) : mParam0(other->mParam0), mParam1(other->mParam1) {}

    CPUParticleEmitterVariable mParam0;
    CPUParticleEmitterVariable mParam1;

    virtual constexpr const char* const getDisplayName() const = 0;
    virtual color4 getDisplayColor() const = 0;
    virtual CPUParticleEmitterVariableVariantTypePair getVariantInput() const = 0;
    virtual CPUparticleEmitterVariableVariantType getOutputType() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterOperation> clone() const = 0;
    virtual BitFlags<ParticleComponentType> getRequiredComponents() const { return BitFlags<ParticleComponentType>(); }

    bool updateAndRenderControls();

    virtual void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) = 0;

    bool loadFromYml(ryml::ConstNodeRef node) override;

protected:
    void evaluateParams(CpuParticleEmitter& emitter, ParticleID id) {
        mParam0.evaluate(emitter, id);
        mParam1.evaluate(emitter, id);
    }

    virtual void saveYmlData(ryml::NodeRef node) const override;
};

#define SIMPLE_EXECUTE_OP(T1, T2, OP) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = std::get<T1>(mParam0.mVarData) OP std::get<T2>(mParam1.mVarData); \
    }

#define DEFINE_CPUPEO_BINARY(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, TYPE2, OP) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable(TYPE2(0))) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable(other.mParam1)) {} \
    NAME(const NAME* other) : CPUParticleEmitterOperation(other) {} \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::TYPE2); } \
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::TYPE1; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(this); } \
    SIMPLE_EXECUTE_OP(TYPE1, TYPE2, OP); \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_SET(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    NAME(const NAME* other) : CPUParticleEmitterOperation(other) {} \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::None); } \
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::TYPE1; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       mParam0.evaluate(emitter, id); \
       output->mVarData = std::get<TYPE1>(mParam0.mVarData); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_NEGATE(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    NAME(const NAME* other) : CPUParticleEmitterOperation(other) {} \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::None); } \
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::TYPE1; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       mParam0.evaluate(emitter, id); \
       output->mVarData = -std::get<TYPE1>(mParam0.mVarData); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_CONVERT(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, CONVERT) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    NAME(const NAME* other) : CPUParticleEmitterOperation(other) {} \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::None); } \
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::CONVERT; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       mParam0.evaluate(emitter, id); \
       output->mVarData = CONVERT(std::get<TYPE1>(mParam0.mVarData)); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_QUERY_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, ...) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    NAME(const NAME* other) : CPUParticleEmitterOperation(other) {} \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    void saveYmlData(ryml::NodeRef node) const  override { } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::None, CPUparticleEmitterVariableVariantType::None); } \
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::TYPE1; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define COLOR_STANDARD color4(0.3f, 0.3f, 0.7f, 1.0f)
#define COLOR_CONVERT color4(0.6f, 0.6f, 0.25f, 1.0f)
#define COLOR_QUERY color4(0.5f, 0.7f, 0.3f, 1.0f)

DEFINE_CPUPEO_BINARY(CPUPEO_AddVec3, "Add Vec3", "add_vec3", COLOR_STANDARD, f32v3, f32v3, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyVec3, "Multiply Vec3", "mult_vec3", COLOR_STANDARD, f32v3, f32v3, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloatToVec3, "Add Float To Vec3", "add_f32_vec3", COLOR_STANDARD, f32v3, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloatToVec3, "Multiply Float To Vec3", "mult_f32_vec3", COLOR_STANDARD, f32v3, f32, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloat, "Add Float", "add_f32", COLOR_STANDARD, f32, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloat, "Multiply Float", "mult_f32", COLOR_STANDARD, f32, f32, *)

DEFINE_CPUPEO_SET(CPUPEO_SetColor, "Set Color", "set_color", COLOR_STANDARD, color4)
DEFINE_CPUPEO_SET(CPUPEO_SetVec4, "Set Vec4", "set_vec4", COLOR_STANDARD, f32v4)
DEFINE_CPUPEO_SET(CPUPEO_SetVec3, "Set Vec3", "set_vec3", COLOR_STANDARD, f32v3)
DEFINE_CPUPEO_SET(CPUPEO_SetVec2, "Set Vec2", "set_vec2", COLOR_STANDARD, f32v2)
// CONTINUE HERE
DEFINE_CPUPEO_SET(CPUPEO_SetFloat, "Set Float", "set_f32", COLOR_STANDARD, f32)
DEFINE_CPUPEO_SET(CPUPEO_SetUInt, "Set UInt", "set_uint", COLOR_STANDARD, ui32)

DEFINE_CPUPEO_NEGATE(CPUPEO_NegateVec3, "Negate Vec3", "negate_vec3", COLOR_STANDARD, f32v3)
DEFINE_CPUPEO_NEGATE(CPUPEO_NegateVec2, "Negate Vec2", "negate_vec2", COLOR_STANDARD, f32v2)
DEFINE_CPUPEO_NEGATE(CPUPEO_NegateFloat, "Negate Float", "negate_f32", COLOR_STANDARD, f32)

DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec4, "Float To Vec4", "f32_to_vec4", COLOR_CONVERT, f32, f32v4)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec3, "Float To Vec3", "f32_to_vec3", COLOR_CONVERT, f32, f32v3)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec2, "Float To Vec2", "f32_to_vec2", COLOR_CONVERT, f32, f32v2)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToUInt, "Float To UInt", "f32_to_uint", COLOR_CONVERT, f32, ui32)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertUIntToFloat, "UInt To Float", "uint_to_f32", COLOR_CONVERT, f32, ui32)

DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryPosition, "Particle Position", "p_pos", COLOR_QUERY, f32v3)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryVelocity, "Particle Velocity", "p_vel", COLOR_QUERY, f32v3,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Velocity; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryScale, "Particle Scale", "p_scale", COLOR_QUERY, f32v2,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Scale; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryRotation, "Particle Rotation", "p_rot", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Rotation; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryNormalizedLifetime, "Particle Normalized Lifetime", "p_norm_life", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return {}; }
)

#undef COLOR_STANDARD
#undef COLOR_CONVERT
#undef COLOR_QUERY

static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);