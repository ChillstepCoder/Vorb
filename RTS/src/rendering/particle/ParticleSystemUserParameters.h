#pragma once

using ParticleSystemUserParameter = std::variant<ui32, f32, f32v2, f32v3>;
using ParticleSystemUserParameterMap = UnorderedFlatMap<StrToken, ParticleSystemUserParameter>;