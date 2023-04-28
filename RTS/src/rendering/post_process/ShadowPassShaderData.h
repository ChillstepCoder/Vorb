#pragma once

constexpr int MAX_SHADOW_CASCADE_LEVELS = 4;
struct ShadowPassShaderData {
    const f32m4* shadowFrustumMatrices;
    const f32* shadowCascadePlaneDistances;
    VGTexture shadowMap;
};