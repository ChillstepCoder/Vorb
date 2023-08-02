#pragma once

class CPUParticleEmitterOperation;
class CpuParticleEmitter;

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

    // TODO: pool allocate?
    std::unique_ptr<CPUParticleEmitterOperation> mOperation = nullptr;
    CPUParticleEmitterVariantData mVarData;
};

// TODO: test perf vs virtual func
//typedef void(*CPUParticleEmitterOperationMethod)(class CpuParticleEmitter& emitter, int particleID, CPUParticleEmitterVariable* p0, CPUParticleEmitterVariable* p1);

class CPUParticleEmitterOperation {
public:
    CPUParticleEmitterOperation() = default;
    CPUParticleEmitterOperation(CPUParticleEmitterVariable p0, CPUParticleEmitterVariable p1) : mParam0(p0.mVarData), mParam1(p1.mVarData) {}

    CPUParticleEmitterVariable mParam0;
    CPUParticleEmitterVariable mParam1;

    virtual constexpr const char* getDisplayName() const = 0;
    virtual color4 getDisplayColor() const = 0;
    virtual CPUParticleEmitterVariableVariantTypePair getVariantInput() const = 0;
    virtual std::unique_ptr<CPUParticleEmitterOperation> clone() const = 0;

    void updateAndRenderControls();

    virtual void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) = 0;
protected:
    void evaluateParams(CpuParticleEmitter& emitter, ParticleID id) {
        mParam0.evaluate(emitter, id);
        mParam1.evaluate(emitter, id);
    }
};

#define SIMPLE_EXECUTE_OP(T1, T2, OP) \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       evaluateParams(emitter, id); \
       output->mVarData = std::get<T1>(mParam0.mVarData) OP std::get<T2>(mParam1.mVarData); \
    }


#define DEFINE_CPUPEO_BINARY(NAME, DISP_NAME, DISP_COLOR, TYPE1, TYPE2, OP) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable(TYPE2(0))) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable(other.mParam1)) {} \
    constexpr const char* getDisplayName() const override { return DISP_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::TYPE2); } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(*this); } \
    SIMPLE_EXECUTE_OP(TYPE1, TYPE2, OP); \
};

#define DEFINE_CPUPEO_SET(NAME, DISP_NAME, DISP_COLOR, TYPE1) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    constexpr const char* getDisplayName() const override { return DISP_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::None); } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(*this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       mParam0.evaluate(emitter, id); \
       output->mVarData = std::get<TYPE1>(mParam0.mVarData); \
    } \
};

#define DEFINE_CPUPEO_CONVERT(NAME, DISP_NAME, DISP_COLOR, TYPE1, CONVERT) \
class NAME : public CPUParticleEmitterOperation { \
public: \
    NAME() : CPUParticleEmitterOperation(CPUParticleEmitterVariable(TYPE1(0)), CPUParticleEmitterVariable()) {} \
    NAME(const NAME& other) : CPUParticleEmitterOperation(CPUParticleEmitterVariable(other.mParam0), CPUParticleEmitterVariable()) {} \
    constexpr const char* getDisplayName() const override { return DISP_NAME; } \
    color4 getDisplayColor() const override { return DISP_COLOR; } \
    CPUParticleEmitterVariableVariantTypePair getVariantInput() const override { \
        return CPUParticleEmitterVariableVariantTypePair(CPUparticleEmitterVariableVariantType::TYPE1, CPUparticleEmitterVariableVariantType::None); } \
    std::unique_ptr<CPUParticleEmitterOperation> clone() const override { return std::make_unique<NAME>(*this); } \
    void execute(CpuParticleEmitter& emitter, ParticleID id, CPUParticleEmitterVariable* output) override { \
       mParam0.evaluate(emitter, id); \
       output->mVarData = CONVERT(std::get<TYPE1>(mParam0.mVarData)); \
    } \
};

#define COLOR_STANDARD color4(0.3f, 0.3f, 0.7f, 1.0f)
#define COLOR_CONVERT color4(0.6f, 0.6f, 0.25f, 1.0f)
#define COLOR_QUERY color4(0.5f, 0.7f, 0.3f, 1.0f)

DEFINE_CPUPEO_BINARY(CPUPEO_AddVec3, "Add Vec3", COLOR_STANDARD, f32v3, f32v3, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyVec3, "Multiply Vec3", COLOR_STANDARD, f32v3, f32v3, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloatToVec3, "Add Float To Vec3", COLOR_STANDARD, f32v3, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloatToVec3, "Multiply Float To Vec3", COLOR_STANDARD, f32v3, f32, *)
DEFINE_CPUPEO_BINARY(CPUPEO_AddFloat, "Add Float", COLOR_STANDARD, f32, f32, +)
DEFINE_CPUPEO_BINARY(CPUPEO_MultiplyFloat, "Multiply Float", COLOR_STANDARD, f32, f32, *)

DEFINE_CPUPEO_SET(CPUPEO_SetColor, "Set Color", COLOR_STANDARD, color4)
DEFINE_CPUPEO_SET(CPUPEO_SetVec4, "Set Vec4", COLOR_STANDARD, f32v4)
DEFINE_CPUPEO_SET(CPUPEO_SetVec3, "Set Vec3", COLOR_STANDARD, f32v3)
DEFINE_CPUPEO_SET(CPUPEO_SetVec2, "Set Vec2", COLOR_STANDARD, f32v2)
DEFINE_CPUPEO_SET(CPUPEO_SetFloat, "Set Float", COLOR_STANDARD, f32)
DEFINE_CPUPEO_SET(CPUPEO_SetUInt, "Set UInt", COLOR_STANDARD, ui32)

DEFINE_CPUPEO_SET(CPUPEO_ConvertFloatToVec4, "Convert Float To Vec4", COLOR_CONVERT, f32v4, f32)
DEFINE_CPUPEO_SET(CPUPEO_ConvertFloatToVec3, "Convert Float To Vec3", COLOR_CONVERT, f32v3, f32)
DEFINE_CPUPEO_SET(CPUPEO_ConvertFloatToVec2, "Convert Float To Vec2", COLOR_CONVERT, f32v2, f32)
DEFINE_CPUPEO_SET(CPUPEO_ConvertFloatToUInt, "Convert Float To UInt", COLOR_CONVERT, f32, ui32)
DEFINE_CPUPEO_SET(CPUPEO_ConvertUIntToFloat, "Convert UInt To Float", COLOR_CONVERT, ui32, f32)

#undef COLOR_STANDARD
#undef COLOR_CONVERT
#undef COLOR_QUERY

static_assert(e_count(CPUparticleEmitterVariableVariantType) == 7);