#pragma once

#include "rendering/post_process/ShadowPassShaderData.h"

enum class ShadowLodDetail {
    None,
    Low,
    Medium,
    High,
    Highest
};
SERIALIZABLE_ENUM_SAME_NAME(ShadowLodDetail,
    pair{ ShadowLodDetail::None, "none"sv},
    pair{ ShadowLodDetail::Low, "low"sv },
    pair{ ShadowLodDetail::Medium, "medium"sv },
    pair{ ShadowLodDetail::High, "high"sv },
    pair{ ShadowLodDetail::Highest, "highest"sv }
);
static_assert(e_cast(ShadowLodDetail::Highest) == MAX_SHADOW_CASCADE_LEVELS);

namespace Shadows {
    inline f32 getMaxDistance(const f32* planeDistances, ShadowLodDetail detail) {
        if (detail == ShadowLodDetail::None) {
            return 0.0f;
        }
        return planeDistances[e_cast(detail) - 1];
    }
};
