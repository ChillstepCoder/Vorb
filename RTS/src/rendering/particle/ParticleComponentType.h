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
