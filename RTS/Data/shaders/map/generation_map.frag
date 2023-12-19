
#include "util/noise/snoise3.glsl"
#include "const/biome_ids.glsl"

in vec2 fUV;

out vec4 fColor;

uniform vec2 unSpawnPoint = vec2(0.5);
uniform float unZoom = 1.0;
uniform bool unShowBiomes = false;
uniform bool unShowHeight = false;


const float HEIGHT_VERTEX_SPACING = 2; // Match C++
const float BIOME_VERTEX_SPACING = 8; // Match C++
const uint BIOME_VERTEX_SPACING_DIFF = uint(BIOME_VERTEX_SPACING) / uint(HEIGHT_VERTEX_SPACING);

layout(binding = 0) uniform sampler2D heightTexture;
layout(binding = 1) uniform sampler2D biomeTexture;

float standardNoise(vec3 position, int octaves, float frequency, float persistence, vec2 posOffset, float amplitude, float heightOffset) {
    position.xy += posOffset;
    return (noise(position, octaves, frequency, persistence) + heightOffset) * amplitude;
}

vec3 colorFromHeight(float height) {
    if (height < 0.0f) {
        const float depthMult = min(-height * 0.025, 1.0);
        return mix(vec3(4.0 / 255.0, 119.0 / 255.0, 162.0 / 255.0), vec3(3.0 / 255.0, 66.0 / 255.0, 122.0 / 255.0), depthMult);
    }
    else {
        if (unShowHeight) {
            const float heightMult = height * 0.1;
            vec3 hColor = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), fract(heightMult));
            return mix(hColor, vec3(1.0, 0.0, 0.0), min(height * 0.005, 0.5));
        } else {
            const float heightMult = min(height * 0.01, 1.0);
            return mix(vec3(40.0 / 255.0, 98.0 / 255.0, 41.0 / 255.0), vec3(1.0), heightMult);
        }
    }
}

void main() {
    
    //ivec2 coords = ivec2(int(fUV.x * float(unHeightDataDims.x)), int(fUV.y * float(unHeightDataDims.y)));
    //coords = clamp(coords, ivec2(0), unHeightDataDims - ivec2(1));
    
    // Height
    float height = (texture(heightTexture, fUV).r * 255.0 - 127.0) + 100.0; 
    fColor.rgb = colorFromHeight(height);
    fColor.a = 1.0;
    
    // Biome
    if (unShowBiomes) {
        int biome = int(round(texture(biomeTexture, fUV).r * 255.0)); 
        fColor.rgb = mix(fColor.rgb, BIOME_COLORS[biome], 0.5);
    }
    
    // Spawn cursor
    float distanceFromSpawn = length(fUV - unSpawnPoint) * unZoom * 0.5;
    float spawnCursorIntensity = max(1.0 - distanceFromSpawn * 300.0, 0.0);
    spawnCursorIntensity = pow(spawnCursorIntensity, 0.6);
    spawnCursorIntensity = smoothstep(0.0, 1.0, spawnCursorIntensity);
    fColor.rgb = mix(fColor.rgb, mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), 1.0 - spawnCursorIntensity), spawnCursorIntensity);
    fColor.rgb = fColor.rgb;
}





/*
float standardNoise(vec3 position, int octaves, float frequency, float persistence, vec2 posOffset, float amplitude, float heightOffset) {
    position.xy += posOffset;
    return (noise(position, octaves, frequency, persistence) + heightOffset) * amplitude;
}

void main() {
    float distanceFromCenter = max(abs(fUV.x - 0.5), abs(fUV.y - 0.5));
    float noiseStrength = pow((1.0 - (2.0 * distanceFromCenter)), 0.5);
    vec2 coords = fUV;
    coords += standardNoise(vec3(coords.x, coords.y, 0.0), 6, 0.25, 0.7, vec2(0, 0), 1.0, 0.0) * noiseStrength;
    coords = clamp(coords, 0.0, 1.0);
    vec3 botCol = mix(colors[0], colors[1], coords.x);
    vec3 topCol = mix(colors[2], colors[3], coords.x);
    vec3 color = mix(topCol, botCol, coords.y);
    
    float len = length(color);
    if (len > 1.0) {
        color = normalize(color);
        len = 1.0;
        color = vec3(1.0, 1.0, 1.0);
    }
    fColor.rgb = color;
    fColor.a = 1.0;
}
*/