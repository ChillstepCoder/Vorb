#pragma once

namespace vorb {
    namespace graphics {
        enum class SamplerStateType {
            POINT_WRAP,
            POINT_CLAMP,
            LINEAR_WRAP,
            LINEAR_CLAMP,
            LINEAR_CLAMP_BORDER,
            POINT_WRAP_MIPMAP,
            POINT_CLAMP_MIPMAP,
            LINEAR_WRAP_MIPMAP,
            LINEAR_CLAMP_MIPMAP,
            LINEAR_CLAMPV,
            LINEAR_CLAMPV_MIPMAP,
            LINEAR_MIRROR,
            COUNT
        };
    }
}
namespace vg = vorb::graphics;