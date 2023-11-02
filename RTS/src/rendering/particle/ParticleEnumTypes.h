#pragma once

// Position is implicit
enum class ParticleComponentType : ui8 {
    Velocity = BIT(0),
    Scale = BIT(1),
    Color = BIT(2),
    HDRColor = BIT(3),
    Lifespan = BIT(4),
    MaterialID = BIT(5),
    Rotation = BIT(6),
    // TODO: SortDepth?
    TERM
};

enum class ParticleBlendMode {
    Additive,
    Subtractive,
    Alpha,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(ParticleBlendMode,
    pair{ ParticleBlendMode::Additive, "additive"sv },
    pair{ ParticleBlendMode::Subtractive, "subtractive"sv },
    pair{ ParticleBlendMode::Alpha, "alpha"sv }
);