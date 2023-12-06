#pragma once

// TODO: BAD

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/ui/GameWindow.h>

SERIALIZABLE_ENUM(vg::SamplerStateType, SamplerStateType,
    pair{ vg::SamplerStateType::POINT_WRAP, "POINT_WRAP"sv },
    pair{ vg::SamplerStateType::POINT_CLAMP, "POINT_CLAMP"sv },
    pair{ vg::SamplerStateType::LINEAR_WRAP, "LINEAR_WRAP"sv },
    pair{ vg::SamplerStateType::LINEAR_CLAMP, "LINEAR_CLAMP"sv },
    pair{ vg::SamplerStateType::LINEAR_CLAMP_BORDER, "LINEAR_CLAMP_BORDER"sv },
    pair{ vg::SamplerStateType::POINT_WRAP_MIPMAP, "POINT_WRAP_MIPMAP"sv },
    pair{ vg::SamplerStateType::POINT_CLAMP_MIPMAP, "POINT_CLAMP_MIPMAP"sv },
    pair{ vg::SamplerStateType::LINEAR_WRAP_MIPMAP, "LINEAR_WRAP_MIPMAP"sv },
    pair{ vg::SamplerStateType::LINEAR_CLAMP_MIPMAP, "LINEAR_CLAMP_MIPMAP"sv },
    pair{ vg::SamplerStateType::LINEAR_CLAMPV, "LINEAR_CLAMPV"sv },
    pair{ vg::SamplerStateType::LINEAR_CLAMPV_MIPMAP, "LINEAR_CLAMPV_MIPMAP"sv },
    pair{ vg::SamplerStateType::LINEAR_MIRROR, "LINEAR_MIRROR"sv }
)
static_assert(e_count(vg::SamplerStateType) == 12, "Update with new");

SERIALIZABLE_ENUM(vg::BlendStateType, BlendStateType,
    pair{ vg::BlendStateType::ALPHA, "alpha"sv },
    pair{ vg::BlendStateType::ALPHA_PREMULTIPLIED, "alpha_premult"sv },
    pair{ vg::BlendStateType::ADDITIVE, "add"sv },
    pair{ vg::BlendStateType::SUBTRACTIVE, "subtract"sv },
    pair{ vg::BlendStateType::REPLACE, "replace"sv },
    pair{ vg::BlendStateType::MULTIPLY, "multiply"sv }
)
static_assert(e_count(vg::BlendStateType) == 6, "Update with new");

SERIALIZABLE_ENUM(vui::GameSwapInterval, GameSwapInterval,
    pair{ vui::GameSwapInterval::UNLIMITED_FPS, "Unlimited"sv },
    pair{ vui::GameSwapInterval::V_SYNC, "VSync"sv },
    pair{ vui::GameSwapInterval::LOW_SYNC, "LowSync"sv },
    pair{ vui::GameSwapInterval::POWER_SAVER, "PowerSaver"sv },
    pair{ vui::GameSwapInterval::USE_VALUE_CAP, "ValueCap"sv }
)
