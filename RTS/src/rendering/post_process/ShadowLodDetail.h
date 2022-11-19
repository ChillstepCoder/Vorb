#pragma once


constexpr int MAX_SHADOW_CASCADE_LEVELS = 4;
enum class ShadowLodDetail {
    None = -1,
    Low,
    Medium,
    High,
    Highest
};
KEG_ENUM_DECL(ShadowLodDetail);
static_assert(e_cast(ShadowLodDetail::Highest) == MAX_SHADOW_CASCADE_LEVELS - 1);


namespace Shadows {
    inline f32 getMaxDistance(const f32* planeDistances, ShadowLodDetail detail) {
        if (detail == ShadowLodDetail::None) {
            return 0.0f;
        }
        return planeDistances[e_cast(detail)];
    }
};
