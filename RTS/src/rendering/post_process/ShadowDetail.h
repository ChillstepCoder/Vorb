#pragma once

#include "rendering/post_process/ShadowPassShaderData.h"

enum class ShadowDetail : ui8 {
    None,
    Low,
    Medium,
    High,
    Highest
};
SERIALIZABLE_ENUM_SAME_NAME(ShadowDetail,
    pair{ ShadowDetail::None, "none"sv },
    pair{ ShadowDetail::Low, "low"sv },
    pair{ ShadowDetail::Medium, "medium"sv },
    pair{ ShadowDetail::High, "high"sv },
    pair{ ShadowDetail::Highest, "highest"sv }
);

enum class ShadowModelDetail : ui8 {
    None,
    Low,
    Medium,
    High,
    Highest
};
SERIALIZABLE_ENUM_SAME_NAME(ShadowModelDetail,
    pair{ ShadowModelDetail::None, "none"sv},
    pair{ ShadowModelDetail::Low, "low"sv },
    pair{ ShadowModelDetail::Medium, "medium"sv },
    pair{ ShadowModelDetail::High, "high"sv },
    pair{ ShadowModelDetail::Highest, "highest"sv },
);

namespace Shadows {
    inline f32 getMaxDistance(const f32* planeDistances, ShadowDetail detail) {
        if (detail == ShadowDetail::None) {
            return 0.0f;
        }
        return planeDistances[e_cast(detail) - 1];
    }
};
