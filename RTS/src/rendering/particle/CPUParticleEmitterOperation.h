#pragma once

class CPUParticleEmitterOperation;
class CpuParticleEmitter;

#include "rendering/particle/ParticleEnumTypes.h"
#include "rendering/particle/ParticleEmitterVariableName.h"

#include "definitions/ParticleEmitterDef.h"
#include "util/RandomPointFromShape.h"

#include "serialization/YmlSerializable.h"

enum class CPUParticleEmitterParameterVariantType : ui8{
    color4,
    f32v4,
    f32v3,
    f32v2,
    f32,
    ui32,
    namedUInt,
    namedFloat,
    namedVec2,
    namedVec3,
    COUNT
};
typedef std::variant<
    color4, f32v4, f32v3, f32v2, f32, ui32, ParticleEmitterVariableNameUInt, ParticleEmitterVariableNameFloat, ParticleEmitterVariableNameVec2, ParticleEmitterVariableNameVec3
> CPUParticleEmitterVariantData;
static_assert(e_count(CPUParticleEmitterParameterVariantType) == 10);

class CPUParticleEmitterParameter {
public:
    CPUParticleEmitterParameter() = default;
    CPUParticleEmitterParameter(CPUParticleEmitterVariantData data) : mVarData(data) {}
    CPUParticleEmitterParameter(std::unique_ptr<CPUParticleEmitterOperation> operation, CPUParticleEmitterVariantData data) : mOperation(std::move(operation)), mVarData(data) {}
    CPUParticleEmitterParameter(const CPUParticleEmitterParameter& other);

    VORB_MOVABLE(CPUParticleEmitterParameter);

    void evaluate(CpuParticleEmitter& emitter, ParticleID id);
    bool updateAndRenderTweaker(const char*const label, const ParticleEmitterDef& parentEmitter);
    bool loadFromYml(ryml::ConstNodeRef node, std::string_view name);
    void saveYmlData(ryml::NodeRef node, std::string_view name) const;

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

#define E_VAR CPUParticleEmitterVariantData

// TODO: test perf vs virtual func
//typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterParameter* p0, CPUParticleEmitterParameter* p1);

class CPUParticleEmitterOperation : public YmlSerializable {
public:
    CPUParticleEmitterOperation() = default;

    virtual constexpr const char* const getDisplayName() const = 0;
    virtual color4 getDisplayColor() const = 0;
    virtual CPUParticleEmitterParameterVariantType getOutputType() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterOperation> clone() const = 0;
    virtual BitFlags<ParticleComponentType> getRequiredComponents() const { return {}; }

    virtual bool updateAndRenderControls(const ParticleEmitterDef& parentEmitter);
    virtual bool updateAndRenderExtraPreControls(const ParticleEmitterDef& parentEmitter) { return false; }
    virtual bool updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) { return false; }

    virtual void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) = 0;

    virtual bool loadFromYml(ryml::ConstNodeRef node) override;
    virtual const char* const getParamName(size_t paramIndex) const = 0;
    virtual size_t getParamIndex(const char* const paramName) const { assert(false); return UINT32_MAX; };

    bool canAddToEmitter(const ParticleEmitterDef& emitter) const {
        const BitFlags<ParticleComponentType> required = getRequiredComponents();
        return (emitter.mActiveComponents & required) == required;
    }

protected:
    std::vector<CPUParticleEmitterParameter> mParams;

    inline void evaluateParams(CpuParticleEmitter& emitter, ParticleID id) {
        for (auto&& p : mParams) {
            p.evaluate(emitter, id);
        }
    }

    virtual void saveYmlData(ryml::NodeRef node) const override;
};

// Usage: Type, E_VAR(type1(default1)), E_VAR(type2(default2)), ...
#define OPERATION_PARAMS(TYPE, ...) \
public: \
    TYPE() { \
        mParams = std::move(std::vector<CPUParticleEmitterParameter>{ __VA_ARGS__ }); \
    }

// Usage: "paramName1", "paramName2", ...
#define OPERATION_PARAM_NAMES(...) \
protected: \
static constexpr const char* const paramNames[] = { __VA_ARGS__ }; \
public: \
    const char* const getParamName(size_t paramIndex) const override { \
        return paramNames[paramIndex]; \
    } \
    size_t getParamIndex(const char* const paramName) const override { \
        for (size_t i = 0; i < std::size(paramNames); ++i) { \
            if (strcmp(paramName, paramNames[i]) == 0) { \
                return i; \
            } \
        } \
        assert(false && "Could not find param name"); \
        return SIZE_MAX; \
    }


#define OPERATION_NO_PARAMS() \
public: \
    const char* const getParamName(size_t paramIndex) const override { \
        assert(false && "No params"); \
        return nullptr; \
    } \
    size_t getParamIndex(const char* const paramName) const override { \
        assert(false && "No params"); \
        return SIZE_MAX; \
    }

#define DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
public: \
    constexpr const char* const getDisplayName() const override { return DISP_NAME; } \
    constexpr const char* const getYmlName() const override { return YML_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(*this); } \
    CPUParticleEmitterParameterVariantType getOutputType() const override { return CPUParticleEmitterParameterVariantType::OUTPUT_TYPE; }

#define DEFINE_CPUPEO_BINARY(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, TYPE2, OP) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_PARAMS(NAME, E_VAR(TYPE1(0)), E_VAR(TYPE2(0))) \
    OPERATION_PARAM_NAMES("p0", "p1") \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
     void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = std::get<TYPE1>(mParams[0].mVarData) OP std::get<TYPE2>(mParams[1].mVarData); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_NEGATE(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_PARAMS(NAME, E_VAR(TYPE1(0))) \
    OPERATION_PARAM_NAMES("v") \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = -std::get<TYPE1>(mParams[0].mVarData); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_CONVERT(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, CONVERT) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_PARAMS(NAME, E_VAR(TYPE1(0))) \
    OPERATION_PARAM_NAMES("v") \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, CONVERT) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = CONVERT(std::get<TYPE1>(mParams[0].mVarData)); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_QUERY_COMPONENT_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE, ...) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_NO_PARAMS() \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_QUERY_VARIABLE_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE, VARTYPE, ...) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_PARAMS(NAME, E_VAR(VARTYPE(0))) \
    OPERATION_PARAM_NAMES("name") \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

// Returns type1 always
#define DEFINE_CPUPEO_CUSTOM_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE, ...) \
class NAME : public CPUParticleEmitterOperation { \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterParameter* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define COLOR_STANDARD color4(0.3f, 0.3f, 0.7f, 1.0f)
#define COLOR_CONVERT color4(0.6f, 0.6f, 0.25f, 1.0f)
#define COLOR_QUERY color4(0.5f, 0.7f, 0.3f, 1.0f)
#define COLOR_CURVE color4(0.3f, 0.7f, 0.7f, 1.0f)
#define COLOR_COMPLEX color4(1.0f, 0.3f, 0.3f, 1.0f)
#define COLOR_INPUT color4(0.65f, 0.85f, 0.65f, 1.0f)
#define COLOR_VARIABLE color4(0.0f, 0.85f, 0.0f, 1.0f)

DEFINE_CPUPEO_BINARY(CPUPEO_AddVec2, "Add Vec2", "add_vec2", COLOR_STANDARD, f32v2, f32v2, +)
DEFINE_CPUPEO_BINARY(CPUPEO_SubVec2, "Subtract Vec2", "sub_vec2", COLOR_STANDARD, f32v2, f32v2, -)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyVec2, "Multiply Vec2", "mult_vec2", COLOR_STANDARD, f32v2, f32v2, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloatToVec2, "Add Float To Vec2", "add_f32_vec2", COLOR_STANDARD, f32v2, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloatToVec2, "Multiply Float To Vec2", "mult_f32_vec2", COLOR_STANDARD, f32v2, f32, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddVec3, "Add Vec3", "add_vec3", COLOR_STANDARD, f32v3, f32v3, +)
DEFINE_CPUPEO_BINARY(CPUPEO_SubVec3, "Subtract Vec3", "sub_vec3", COLOR_STANDARD, f32v3, f32v3, -)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyVec3, "Multiply Vec3", "mult_vec3", COLOR_STANDARD, f32v3, f32v3, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloatToVec3, "Add Float To Vec3", "add_f32_vec3", COLOR_STANDARD, f32v3, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloatToVec3, "Multiply Float To Vec3", "mult_f32_vec3", COLOR_STANDARD, f32v3, f32, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloat, "Add Float", "add_f32", COLOR_STANDARD, f32, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_SubFloat, "Subtract Float", "sub_f32", COLOR_STANDARD, f32, f32, -)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloat, "Multiply Float", "mult_f32", COLOR_STANDARD, f32, f32, *)

DEFINE_CPUPEO_NEGATE(CPUPEO_NegateVec3, "Negate Vec3", "negate_vec3", COLOR_STANDARD, f32v3)
DEFINE_CPUPEO_NEGATE(CPUPEO_NegateVec2, "Negate Vec2", "negate_vec2", COLOR_STANDARD, f32v2)
DEFINE_CPUPEO_NEGATE(CPUPEO_NegateFloat, "Negate Float", "negate_f32", COLOR_STANDARD, f32)

DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec4, "Float To Vec4", "f32_to_vec4", COLOR_CONVERT, f32, f32v4)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec3, "Float To Vec3", "f32_to_vec3", COLOR_CONVERT, f32, f32v3)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToVec2, "Float To Vec2", "f32_to_vec2", COLOR_CONVERT, f32, f32v2)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertFloatToUInt, "Float To UInt", "f32_to_uint", COLOR_CONVERT, f32, ui32)
DEFINE_CPUPEO_CONVERT(CPUPEO_ConvertUIntToFloat, "UInt To Float", "uint_to_f32", COLOR_CONVERT, f32, ui32)

DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QueryPosition, "Particle Position", "p_pos", COLOR_QUERY, f32v3)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QueryVelocity, "Particle Velocity", "p_vel", COLOR_QUERY, f32v3,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Velocity; }
)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QuerySpeed, "Particle Speed", "p_vel", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Velocity; }
)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QueryScale, "Particle Scale", "p_scale", COLOR_QUERY, f32v2,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Scale; }
)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QueryRotation, "Particle Rotation", "p_rot", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Rotation; }
)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_QueryNormalizedLifetime, "Particle Normalized Lifetime", "p_norm_life", COLOR_QUERY, f32)

// Inputs
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_InputImpactDirection, "(In) Impact Direction", "i_idir", COLOR_INPUT, f32v3)
DEFINE_CPUPEO_QUERY_COMPONENT_DECL(CPUPEO_InputImpactSurfaceNormal, "(In) Impact Surface Normal", "i_inorm", COLOR_INPUT, f32v3)

// Varible queries
DEFINE_CPUPEO_QUERY_VARIABLE_DECL(CPUPEO_UIntVariable, "Get Named UInt", "v_uint", COLOR_VARIABLE, ui32, ParticleEmitterVariableNameUInt)
DEFINE_CPUPEO_QUERY_VARIABLE_DECL(CPUPEO_FloatVariable, "Get Named Float", "v_float", COLOR_VARIABLE, f32, ParticleEmitterVariableNameFloat)
DEFINE_CPUPEO_QUERY_VARIABLE_DECL(CPUPEO_Vec2Variable, "Get Named Vec2", "v_vec2", COLOR_VARIABLE, f32v2, ParticleEmitterVariableNameVec2)
DEFINE_CPUPEO_QUERY_VARIABLE_DECL(CPUPEO_Vec3Variable, "Get Named Vec3", "v_vec3", COLOR_VARIABLE, f32v3, ParticleEmitterVariableNameVec3)


DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_RandomFloatInRange, "Random Float In Range", "rand_float", COLOR_STANDARD, f32,
    OPERATION_PARAMS(CPUPEO_RandomFloatInRange, E_VAR(f32(0.f)), E_VAR(f32(1.f)))
    OPERATION_PARAM_NAMES("min", "max")
    virtual bool updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
 protected:
    bool mSeedByParticleID = true;
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_ColorCurve, "Color Curve", "color_curve", COLOR_CURVE, color4,
    OPERATION_PARAMS(CPUPEO_ColorCurve, CPUParticleEmitterParameter(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
 protected:
    std::vector<std::pair<f32, color4>> mKeys = { {0.f, color4(255, 255, 255, 255)}, {1.f, color4(255, 255, 255, 255)} };
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_HdrColorCurve, "HDR Color Curve", "hdr_curve", COLOR_CURVE, f32v4,
    OPERATION_PARAMS(CPUPEO_HdrColorCurve, CPUParticleEmitterParameter(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
protected:
    std::vector<std::pair<f32, f32v4>> mKeys = { {0.f, f32v4(1.0f)}, {1.f, f32v4(1.0f)} };
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_FloatCurve, "Float Curve", "float_curve", COLOR_CURVE, f32,
    OPERATION_PARAMS(CPUPEO_FloatCurve, CPUParticleEmitterParameter(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls(const ParticleEmitterDef& parentEmitter) override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
protected:
    std::vector<std::pair<f32, f32>> mKeys = { {0.f, f32(0.0f)}, {1.f, f32(1.0f)} };
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_NormalizeVec3, "Normalize Vec3", "norm_vec3", COLOR_STANDARD, f32v3,
    OPERATION_PARAMS(CPUPEO_NormalizeVec3, E_VAR(f32v3(1.0f, 0.0f, 0.0f)))
    OPERATION_PARAM_NAMES("v")
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_RandomPointInShape, "Random Point In Shape", "rnd_point_shape", COLOR_COMPLEX, f32v3,
    OPERATION_PARAMS(CPUPEO_RandomPointInShape, E_VAR(f32(1.0f)))
public:
    const char* const getParamName(size_t paramIndex) const override;
    virtual bool updateAndRenderExtraPreControls(const ParticleEmitterDef& parentEmitter) override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
protected:
    void saveYmlData(ryml::NodeRef node) const override;
    QueryPointFromShapeType mShapeType = QueryPointFromShapeType::Sphere;
    void onShapeTypeUpdated();
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_ClampFloat, "Clamp Float", "clamp_float", COLOR_STANDARD, f32,
    OPERATION_PARAMS(CPUPEO_ClampFloat, E_VAR(f32(0.f)), E_VAR(f32v2(0.f, 1.0f)))
    OPERATION_PARAM_NAMES("val", "clampRange")
)
DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_ClampVec2, "Clamp Vec2", "clamp_vec2", COLOR_STANDARD, f32v2,
    OPERATION_PARAMS(CPUPEO_ClampVec2, E_VAR(f32v2(0.f)), E_VAR(f32v2(0.f, 1.0f)))
    OPERATION_PARAM_NAMES("val", "clampRange")
)
DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_ClampVec3, "Clamp Vec3", "clamp_vec3", COLOR_STANDARD, f32v2,
    OPERATION_PARAMS(CPUPEO_ClampVec3, E_VAR(f32v3(0.f)), E_VAR(f32v2(0.f, 1.0f)))
    OPERATION_PARAM_NAMES("val", "clampRange")
)
DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_MakeVec2, "Make Vec2", "make_vec2", COLOR_STANDARD, f32v2,
    OPERATION_PARAMS(CPUPEO_MakeVec2, E_VAR(f32(0.f)), E_VAR(f32(0.f)))
    OPERATION_PARAM_NAMES("x", "y")
)
DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_MakeVec3, "Make Vec3", "make_vec3", COLOR_STANDARD, f32v3,
    OPERATION_PARAMS(CPUPEO_MakeVec3, E_VAR(f32(0.f)), E_VAR(f32(0.f)), E_VAR(f32(0.f)))
    OPERATION_PARAM_NAMES("x", "y", "z")
)

#undef COLOR_STANDARD
#undef COLOR_CONVERT
#undef COLOR_QUERY
#undef E_VAR