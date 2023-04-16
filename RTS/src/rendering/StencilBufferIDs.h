#pragma once

enum class StencilBufferIDs : ui8 {
    GEOMETRY = 1,
    TERRAIN = 2,
    SMUDGE = 3,
    CLOUD_OR_WATER = 4,
    SKY = 5
};

constexpr ui8 PAINT_SMUDGE_STENCIL_BUFFER_MASK = e_cast(StencilBufferIDs::TERRAIN) | e_cast(StencilBufferIDs::SMUDGE);