
#include "const/biome_ids.glsl"
#include "util/noise/snoise3.glsl"
#include "util/noise/cellular3d.glsl"

const float MIN_WORLD_GEN_HEIGHT = -3000.0;
const float MAX_WORLD_GEN_HEIGHT = 3000.0;
const float HEIGHT_VERTEX_SPACING = 2; // Match C++
const float BIOME_VERTEX_SPACING = 8; // Match C++
const uint BIOME_VERTEX_SPACING_DIFF = uint(BIOME_VERTEX_SPACING) / uint(HEIGHT_VERTEX_SPACING);
const float MAX_RADIUS = 15000.0;
const float WORLD_RADIUS = 16536.0;
const float WORLD_RADIUS2 = WORLD_RADIUS * WORLD_RADIUS;
const float OUTER_LERP_DISTANCE = WORLD_RADIUS - MAX_RADIUS;
const float WORLD_EDGE_MIN_DEPTH = -100.0;

const float MOUNTAIN_BLEND = 0.3;
const float SEA_BLEND_TO_BIOME = 0.1;
const float SEA_BLEND_TO_MOUNTAINS = 0.15;

float smootherstep(float edge0, float edge1, float x) {
    x = clamp((x - edge0)/(edge1 - edge0), 0., 1.);
    return x * x * x * (x * (x * 6. - 15.) + 10.);
}

float standardNoise(vec3 position, int octaves, float frequency, float persistence, vec2 posOffset, float amplitude, float heightOffset) {
    position.xy += posOffset;
    return (noise(position, octaves, frequency, persistence) + heightOffset) * amplitude;
}
float ridgedNoise(vec3 position, int octaves, float frequency, float persistence, vec2 posOffset, float amplitude, float heightOffset) {
    position.xy += posOffset;
    float noiseVal = noise(position, octaves, frequency, persistence);
    noiseVal = 1.0 - abs(noiseVal);
    return (noiseVal + heightOffset) * amplitude;
}

float computeBaseHeight(vec3 xyPosAndSeed) {
    return standardNoise(xyPosAndSeed, 4, 0.001, 0.7, vec2(0, 0), 10.0, 1.0);
}

float getMountainDist(vec3 xyPosAndSeed, float height) {
    const float seaHeightMultMountains = clamp(height * SEA_BLEND_TO_MOUNTAINS, 0.0, 1.0);
    return standardNoise(xyPosAndSeed.yzx, 5, 0.000166, 0.6, vec2(0, 0), 4.0, -0.15) * seaHeightMultMountains;
}
// Mountains override everything
void applyMountainHeight(vec3 xyPosAndSeed, float mountainDist, float multiplier, inout float height, inout float bestWeight, inout int bestBiome) {
    if (mountainDist > 0.0) {
        const float mountainWeight = min(mountainDist, 1.0);
        const float mountainHeight = cellularNoise3D(xyPosAndSeed.zxy, 5, 0.00157, 0.65, vec2(0, 0), 300.0, 0.0);
        const float mountainDetail = ridgedNoise(xyPosAndSeed.zxy, 8, 0.001, 0.6, vec2(0, 0), 64.0, 0.0);
        height += (mountainHeight + mountainDetail) * mountainWeight * multiplier;
        bestWeight = mountainWeight;
        bestBiome = BIOME_Mountains;
    }
}

// Plains get overridden by everything
void applyPlainsHeight(vec3 xyPosAndSeed, float nonMountainWeight, float multiplier, inout float height, inout float bestWeight, inout int bestBiome) {
    // Plains
    const float plainsHeight = standardNoise(xyPosAndSeed, 6, 0.001, 0.65, vec2(0, 0), 30.0, 0.35);
    height += plainsHeight * nonMountainWeight * multiplier;
    if (nonMountainWeight > bestWeight) {
        bestBiome = BIOME_Plains;
        bestWeight = 0.0;
    }
}

float getForestDist(vec3 xyPosAndSeed) {
    return standardNoise(xyPosAndSeed.yxz, 6, 0.0005, 0.6, vec2(-1000.0, 0), 5.0, -0.2);
}
void applyForestHeight(vec3 xyPosAndSeed, float forestDist, float nonMountainWeight, float multiplier, inout float height, inout float bestWeight, inout int bestBiome) {
    const float forestHeight = standardNoise(xyPosAndSeed, 3, 0.025, 0.65, vec2(0, 0), 5.0, 0.0);
    float forestWeight = nonMountainWeight * forestDist;
    forestWeight = clamp(forestWeight, 0.0, 1.0);
    height += forestHeight * forestWeight * multiplier;
    if (forestWeight > bestWeight) {
        bestBiome = BIOME_Forest;
        bestWeight = forestWeight;
    }
}

float getHotSpringsDist(vec3 xyPosAndSeed) {
    return standardNoise(xyPosAndSeed.zxy, 6, 0.0005, 0.6, vec2(1000.0, 0), 5.0, -0.25);
}
void applyHotSpringsHeight(vec3 xyPosAndSeed, float springsDist, float nonMountainWeight, float multiplier, inout float height, inout float bestWeight, inout int bestBiome) {
    float springsHeight = standardNoise(xyPosAndSeed, 3, 0.008, 0.65, vec2(0, 0), 26.0, -0.5);
    float springsWeight = nonMountainWeight * springsDist;
    // Springs lower priority
    springsWeight *= (1.0 - bestWeight);
    springsWeight = clamp(springsWeight, 0.0, 1.0);
    springsHeight = springsHeight * springsWeight * multiplier;
    // Smooth stepped function
    const float stepDistance = 3.0;
    float steppedHeight = floor(springsHeight / stepDistance) * stepDistance;
    springsHeight = steppedHeight + smootherstep(0.0, stepDistance, springsHeight - steppedHeight) * stepDistance;
    height += springsHeight;
    if (springsWeight > bestWeight) {
        bestBiome = BIOME_Hotsprings;
        bestWeight = springsWeight;
    }
}