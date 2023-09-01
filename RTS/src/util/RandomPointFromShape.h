#pragma once
#include "serialization/YmlSerializer.h"

// TODO: Helper?
enum class QueryPointFromShapeType : int {
    Sphere,
    Box,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(QueryPointFromShapeType,
    pair{ QueryPointFromShapeType::Sphere, "sphere"sv },
    pair{ QueryPointFromShapeType::Box, "box"sv }
)
static_assert(e_count(QueryPointFromShapeType) == 2);

struct SphereShapePointQueryData {
    f32 radius = 1.0f;
};
struct BoxShapePointQueryData {
    f32v3 halfExtents = f32v3(1.0f);
};
static_assert(e_count(QueryPointFromShapeType) == 2);

typedef std::variant<SphereShapePointQueryData, BoxShapePointQueryData> PointFromShapeQueryDataVariant;
typedef std::function<f32v3(PointFromShapeQueryDataVariant)> RandomPointFromShapeFunction;

namespace util {
    // MAX seed = random seed
    f32v3 queryRandomPointFromShape(PointFromShapeQueryDataVariant data, ui32 seed = UINT32_MAX);
    // MAX seed = random seed
    RandomPointFromShapeFunction getQueryRandomPointFromShapeFunction(PointFromShapeQueryDataVariant data, ui32 seed = UINT32_MAX);
}