#pragma once

class CPUParticleEmitterOperation;
class CpuParticleEmitter;

#include "rendering/particle/ParticleEnumTypes.h"

#include "serialization/YmlSerializable.h"

#include "util/RandomPointFromShape.h"

enum class CPUparticleEmitterVariableVariantType : ui8{
    color4,
    f32v4,
    f32v3,
    f32v2,
    f32,
    ui32,
    COUNT
};
typedef std::variant<color4, f32v4, f32v3, f32v2, f32, ui32> CPUParticleEmitterVariantData;
static_assert(e_count(CPUparticleEmitterVariableVariantType) == 6);

class CPUParticleEmitterVariable {
public:
    CPUParticleEmitterVariable() = default;
    CPUParticleEmitterVariable(CPUParticleEmitterVariantData data) : mVarData(data) {}
    CPUParticleEmitterVariable(std::unique_ptr<CPUParticleEmitterOperation> operation, CPUParticleEmitterVariantData data) : mOperation(std::move(operation)), mVarData(data) {}
    CPUParticleEmitterVariable(const CPUParticleEmitterVariable& other);

    VORB_MOVABLE(CPUParticleEmitterVariable);

    void evaluate(CpuParticleEmitter& emitter, ParticleID id);
    bool updateAndRenderTweaker(const char*const label);
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
//typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation : public YmlSerializable {
public:
    CPUParticleEmitterOperation() = default;

    virtual constexpr const char* const getDisplayName() const = 0;
    virtual color4 getDisplayColor() const = 0;
    virtual CPUparticleEmitterVariableVariantType getOutputType() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterOperation> clone() const = 0;
    virtual BitFlags<ParticleComponentType> getRequiredComponents() const { return {}; }

    virtual bool updateAndRenderControls();
    virtual bool updateAndRenderExtraPreControls() { return false; }
    virtual bool updateAndRenderExtraPostControls() { return false; }

    virtual void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) = 0;

    virtual bool loadFromYml(ryml::ConstNodeRef node) override;
    virtual const char* const getParamName(size_t paramIndex) const = 0;
    virtual size_t getParamIndex(const char* const paramName) const { assert(false); return UINT32_MAX; };

protected:
    std::vector<CPUParticleEmitterVariable> mParams;

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
        mParams = std::move(std::vector<CPUParticleEmitterVariable>{ __VA_ARGS__ }); \
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
    CPUparticleEmitterVariableVariantType getOutputType() const override { return CPUparticleEmitterVariableVariantType::OUTPUT_TYPE; }

#define DEFINE_CPUPEO_BINARY(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1, TYPE2, OP) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_PARAMS(NAME, E_VAR(TYPE1(0)), E_VAR(TYPE2(0))) \
    OPERATION_PARAM_NAMES("p0", "p1") \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, TYPE1) \
     void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
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
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
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
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = CONVERT(std::get<TYPE1>(mParams[0].mVarData)); \
    } \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define DEFINE_CPUPEO_QUERY_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE, ...) \
class NAME : public CPUParticleEmitterOperation { \
    OPERATION_NO_PARAMS() \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

// Returns type1 always
#define DEFINE_CPUPEO_CUSTOM_DECL(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE, ...) \
class NAME : public CPUParticleEmitterOperation { \
    DEFINE_CPUPEO_COMMON_PARTS(NAME, DISP_NAME, YML_NAME, DISP_COLOR, OUTPUT_TYPE) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override; \
    __VA_ARGS__ \
}; \
REGISTER_YML_OBJECT(YML_NAME, NAME, CPUParticleEmitterOperation);

#define COLOR_STANDARD color4(0.3f, 0.3f, 0.7f, 1.0f)
#define COLOR_CONVERT color4(0.6f, 0.6f, 0.25f, 1.0f)
#define COLOR_QUERY color4(0.5f, 0.7f, 0.3f, 1.0f)
#define COLOR_CURVE color4(0.3f, 0.7f, 0.7f, 1.0f)
#define COLOR_COMPLEX color4(1.0f, 0.3f, 0.3f, 1.0f)

DEFINE_CPUPEO_BINARY(CPUPEO_AddVec3, "Add Vec3", "add_vec3", COLOR_STANDARD, f32v3, f32v3, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyVec3, "Multiply Vec3", "mult_vec3", COLOR_STANDARD, f32v3, f32v3, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloatToVec3, "Add Float To Vec3", "add_f32_vec3", COLOR_STANDARD, f32v3, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloatToVec3, "Multiply Float To Vec3", "mult_f32_vec3", COLOR_STANDARD, f32v3, f32, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloat, "Add Float", "add_f32", COLOR_STANDARD, f32, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloat, "Multiply Float", "mult_f32", COLOR_STANDARD, f32, f32, *)

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
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QuerySpeed, "Particle Speed", "p_vel", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Velocity; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryScale, "Particle Scale", "p_scale", COLOR_QUERY, f32v2,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Scale; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryRotation, "Particle Rotation", "p_rot", COLOR_QUERY, f32,
    BitFlags<ParticleComponentType> getRequiredComponents() const override { return ParticleComponentType::Rotation; }
)
DEFINE_CPUPEO_QUERY_DECL(CPUPEO_QueryNormalizedLifetime, "Particle Normalized Lifetime", "p_norm_life", COLOR_QUERY, f32)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_RandomFloatInRange, "Random Float In Range", "rand_float", COLOR_STANDARD, f32,
    OPERATION_PARAMS(CPUPEO_RandomFloatInRange, E_VAR(f32(0.f)), E_VAR(f32(1.f)))
    OPERATION_PARAM_NAMES("min", "max")
    virtual bool updateAndRenderExtraPostControls() override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
 protected:
    bool mSeedByParticleID = true;
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_ColorCurve, "Color Curve", "color_curve", COLOR_CURVE, color4,
    OPERATION_PARAMS(CPUPEO_ColorCurve, CPUParticleEmitterVariable(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls() override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
 protected:
    std::vector<std::pair<f32, color4>> mKeys = { {0.f, color4(255, 255, 255, 255)}, {1.f, color4(255, 255, 255, 255)} };
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_HdrColorCurve, "HDR Color Curve", "hdr_curve", COLOR_CURVE, f32v4,
    OPERATION_PARAMS(CPUPEO_HdrColorCurve, CPUParticleEmitterVariable(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls() override;
    bool loadFromYml(ryml::ConstNodeRef node) override;
protected:
    std::vector<std::pair<f32, f32v4>> mKeys = { {0.f, f32v4(1.0f)}, {1.f, f32v4(1.0f)} };
    void saveYmlData(ryml::NodeRef node) const override;
)

DEFINE_CPUPEO_CUSTOM_DECL(CPUPEO_FloatCurve, "Float Curve", "float_curve", COLOR_CURVE, f32,
    OPERATION_PARAMS(CPUPEO_FloatCurve, CPUParticleEmitterVariable(std::make_unique<CPUPEO_QueryNormalizedLifetime>(), f32(0.f)))
    OPERATION_PARAM_NAMES("norm_input")
    virtual bool updateAndRenderExtraPostControls() override;
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
    virtual bool updateAndRenderExtraPreControls() override;
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