#pragma once

// For masks to work we use bits, but stencil doesnt have to be just bits
enum class StencilBufferIDs : ui8 {
    GEOMETRY = BIT(0),
    TERRAIN = BIT(1), // TODO: We dont  really  need terrain we can combine with smudge if we run out of bits
    SMUDGE = BIT(2),
    CLOUD_OR_WATER = BIT(3),
    SKY = BIT(4),
    HIGHLIGHT_CORE = BIT(5),

    // NO MORE THAN BIT(7)
};

constexpr ui8 PAINT_SMUDGE_STENCIL_BUFFER_MASK = e_cast(StencilBufferIDs::TERRAIN) | e_cast(StencilBufferIDs::SMUDGE);