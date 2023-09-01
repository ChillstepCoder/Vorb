#include "stdafx.h"
#include "RandomPointFromShape.h"

#include "math/Random.h"

f32v3 util::queryRandomPointFromShape(PointFromShapeQueryDataVariant data, ui32 seed /* = UINT32_MAX*/) {
    return getQueryRandomPointFromShapeFunction(data, seed)(data);
}

RandomPointFromShapeFunction util::getQueryRandomPointFromShapeFunction(PointFromShapeQueryDataVariant data, ui32 seed /* = UINT32_MAX*/) {

    if (seed == UINT32_MAX) {
        seed = Random::getCachedRandom();
    }

    if (std::holds_alternative<SphereShapePointQueryData>(data))
        return [seed](PointFromShapeQueryDataVariant data) -> f32v3 {
            auto& sphereData = std::get<SphereShapePointQueryData>(data);
            f32 theta = 2.f * M_PIF * Random::getCachedRandomfSpecific(seed); // azimuthal angle
            f32 phi = acosf(2.f * Random::getCachedRandomfSpecific(seed + 100) - 1.f); // polar angle
            f32 r = sphereData.radius * Random::getCachedRandomfSpecific(seed + 200); // cube root to ensure points are uniformly distributed

            f32v3 point;
            point.x = r * sin(phi) * cos(theta);
            point.y = r * sin(phi) * sin(theta);
            point.z = r * cos(phi);
            return point;
        };
    else if (std::holds_alternative<BoxShapePointQueryData>(data)) {
        return [seed](PointFromShapeQueryDataVariant data) -> f32v3 {
            auto& boxData = std::get<BoxShapePointQueryData>(data);
            f32 xAlpha = Random::getCachedRandomfSpecific(seed) * 2.0f - 1.0f;
            f32 yAlpha = Random::getCachedRandomfSpecific(seed + 100) * 2.0f - 1.0f;
            f32 zAlpha = Random::getCachedRandomfSpecific(seed + 200) * 2.0f - 1.0f;
         
            f32v3 point;
            point.x = boxData.halfExtents.x * xAlpha;
            point.y = boxData.halfExtents.y * yAlpha;
            point.z = boxData.halfExtents.z * zAlpha;
            return point;
        };
    }
    static_assert(e_count(QueryPointFromShapeType) == 2);
}
