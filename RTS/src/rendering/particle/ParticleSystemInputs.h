#pragma once

struct ParticleSystemInputs {
    f32v3 mInputImpactDirection = f32v3(0.0f, 0.0f, 1.0f);
    f32v3 mInputImpactSurfaceNormal = f32v3(0.0f, 1.0f, 0.0f);
};