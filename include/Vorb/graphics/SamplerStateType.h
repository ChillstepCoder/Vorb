#pragma once

#include <Vorb/io/Keg.h>

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
            COUNT
        };
        KEG_ENUM_DECL(SamplerStateType);
    }
}
namespace vg = vorb::graphics;