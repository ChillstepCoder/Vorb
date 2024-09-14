#include "stdafx.h"
#include "ParticleEnumTypes.h"

SERIALIZABLE_ENUM_SAME_NAME(ParticleBlendMode,
    pair{ ParticleBlendMode::Opaque, "opaque"sv },
    pair{ ParticleBlendMode::Alpha, "alpha"sv },
    pair{ ParticleBlendMode::Additive, "additive"sv },
    pair{ ParticleBlendMode::Subtractive, "subtractive"sv }
);