//#include "util/hsv.glsl"
#include "GlobalUbo.glsl"

#include "util/noise/snoise3.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform sampler2D CellNoise;
uniform sampler2D TurbulentNoise;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 100.0 / 255.0, 155.0 / 255.0);
uniform vec3 StoneColor = vec3(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);
uniform float unColorMapScale = 0.1;

uniform sampler2D unBiomeTexture;


uniform float unHeightMult = 0.191;
uniform float unWavyMult = 0.167;
uniform float unSquaresIntensity = 0.5;
uniform float unSquaresPeriod = 0.187;
uniform float unBlendMult = 0.037;
uniform float unDetailTextureStrength = 1.0;

uniform float unBiomeBlendScale = 1.0;
uniform float unBiomeBlendFrequency = 1.0;

// Config
uniform int unDebugLines = 0;

const float DISTANT_COLOR_EXP = 0.6;
const float DISTANT_COLOR_INTENSITY = 0.75;

in float fHeight;
in vec3 fPosition;
in vec2 fBiomeUV;
in vec2 fUV;
in mat3 fTBN;

uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

// Debug colors
const vec3 BIOME_COLORS[4] = {
    vec3(1.0, 0.0, 0.0), // PLAINS
    vec3(1.0, 0.0, 1.0), // MOUNTAINS
    vec3(0.0, 1.0, 0.0), // FOREST
    vec3(0.0, 1.0, 1.0), // HOT SPRINGS
};

uniform sampler2DArray unBiomeColorMapsTexture;
layout(std430, binding = 4) readonly buffer BiomeColorMapLookup
{
    uint biomeColorMapLookup[];
};

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;


float InvSmoothStep(float x) {
    return x + (x - (x * x * (3.0 - 2.0 * x)));
}

const int NUM_COLORS = 12;

const vec3 GRASS_COLOR = vec3(179.0 / 255.0, 155.0 / 255.0, 112.0 / 255.0);

const vec3 COLORS[NUM_COLORS] = {
    vec3(128.0 / 255.0, 95.0 / 255.0, 106.0 / 255.0),
    vec3(113.0 / 255.0, 79.0 / 255.0, 96.0 / 255.0),
    vec3(188.0 / 255.0, 165.0 / 255.0, 131.0 / 255.0),
    vec3(74.0 / 255.0, 157.0 / 255.0, 139.0 / 255.0),
    vec3(155.0 / 255.0, 141.0 / 255.0, 138.0 / 255.0),
    GRASS_COLOR, // Grass Flat
    GRASS_COLOR, // Grass Mid
    GRASS_COLOR * 0.8, // Grass Steep
    vec3(66.0 / 255.0, 63.0 / 255.0, 56.0 / 255.0),
    vec3(130.0 / 255.0, 145.0 / 255.0, 126.0 / 255.0),
    vec3(160.0 / 255.0, 169.0 / 255.0, 152.0 / 255.0),
    vec3(86.0 / 255.0, 81.0 / 255.0, 75.0 / 255.0),
};

float triangularWave(float val) {
    val = mod(val, 4.0);
    if (val <= 1.0) {
        return val;
    } else if (val <= 3.0) {
        return 2.0 - val;
    } else {
        return val - 4.0;
    }
}

void main() {
	
    // === Crossfade ===
    
    // TODO: Render crossfading with separate material
    vec4 screenPos = (VP * vec4(fPosition, 1.0));
    vec2 screenUV = (screenPos.xy / vec2(screenPos.w));
	float noiseVal = texture(GreyNoise, screenUV * 3.0).r;
    float alpha = unCrossfadeAlpha * 0.05 + noiseVal * unCrossfadeAlpha + 0.4;
	alpha = InvSmoothStep(alpha);
	alpha = min(mix(1.0 - alpha, alpha, unCrossfadeDirection), 1.0);
	alpha = clamp(alpha, 0.0, 1.0);
	
	if (alpha <= 0.5) {
        discard;
    }
    
    // === Normals ===
    // TODO: NORMAL MAPPING
    vec3 normal = vec3(0.0, 0.0, 1.0);
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    

    // TMP Texturing test with color remapping
    // We use the texture value with the terrain color and saturation
    float distance = length(fPosition.xy);
    float distUvLerp = min(distance * 0.001, 1.0);
    
    vec2 farUVs = -(fUV * 0.1);
    float detailValue = mix(texture(GrassTexture, fUV).r, texture(GrassTexture, farUVs).r, distUvLerp) * unDetailTextureStrength;
    //vec3 currhsv = rgb2hsv(oColor.rgb);
    //vec3 texturehsv = rgb2hsv(detailValue);
    //currhsv.b = texturehsv.b;
    //currhsv.b = mix(currhsv.b, texturehsv.b, 0.4);
    //oColor.rgb = hsv2rgb(currhsv);
    
    float preturbX = texture(TurbulentNoise, fBiomeUV * 1000.0 * unBiomeBlendFrequency).r * 2.0 - 1.0;
    float preturbY = texture(TurbulentNoise, -fBiomeUV * 1000.0 * unBiomeBlendFrequency).r * 2.0 - 1.0;
    //float preturbY = -preturbX;
    
    // Texture and color
    int biome = int(round(texture(unBiomeTexture, fBiomeUV + vec2(preturbX, preturbY) * 0.00025 * unBiomeBlendScale).r * 255.0));

    //if (biome == 255) {
    //    oColor.rgb = vec3(1.0,1.0,1.0);
    //}
    
    float cellNoiseColor = texture(CellNoise, fUV * unColorMapScale).r;
    float u = 1.0 - cellNoiseColor;
    float v = detailValue;
    vec2 uv = vec2(u, v);
    vec3 terrainGrad = texture(unBiomeColorMapsTexture, vec3(uv, float(biomeColorMapLookup[biome]))).rgb;
    oColor.rgb = terrainGrad;
   
    //if (biome < 4) {
    //    oColor.rgb = mix(oColor.rgb, BIOME_COLORS[biome], 1.0);
    //}
    
    // =========== Distance color ===========
    vec3 colorNormal = normal.rgb; // normal
    
    float FLAT_REDUCE_MULT = 1.0;
    float GRANULARITY_MULT = unWavyMult;
    float GRANULARITY = 12.0 * GRANULARITY_MULT * (1.0 - colorNormal.z * FLAT_REDUCE_MULT);
    float UV_MOD_MULT = unSquaresPeriod;
    float uvMod = 0.5 + (triangularWave(fUV.x * UV_MOD_MULT) + triangularWave(fUV.y * UV_MOD_MULT)) * unSquaresIntensity * 2.0;
    float HEIGHT_ADD = (fHeight * uvMod) * 0.024 * unHeightMult * 2.0;
    colorNormal = vec3(abs(colorNormal.x) * GRANULARITY + HEIGHT_ADD, abs(colorNormal.y) * GRANULARITY + HEIGHT_ADD, colorNormal.z * GRANULARITY);
    
    float totalNormal = colorNormal.x + colorNormal.y;
    
    float lowIndex = round(totalNormal);
    //float lowIndex = floor(totalNormal);
    float highIndex = ceil(totalNormal);
    //int index = int(round(totalNormal));
    int index = int(lowIndex + 5) % NUM_COLORS;
    int index2 = int(highIndex + 5) % NUM_COLORS;
    
    float lerpVal = mod(totalNormal, 1.0);
    // Shorten the transition
    lerpVal = (lerpVal - 0.3) * 12.0 * unBlendMult;
    lerpVal = clamp(lerpVal, 0.0, 1.0);
    vec3 distanceColor = mix(COLORS[index], COLORS[index2], lerpVal); // PRETTY RAINBOW + 0.5 * vec3((cos(fUV.x * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1 - fUV.x * 0.1) + 1.0) * 0.5);
    oColor.rgb = mix(oColor.rgb, distanceColor, pow(distUvLerp, DISTANT_COLOR_EXP) * DISTANT_COLOR_INTENSITY);
        
        
    if (unDebugLines == 1)
    {
        vec2 scaledUV = fUV * 0.3;
        vec2 uv = vec2(fract(scaledUV.x), fract(scaledUV.y));
        if (uv.x > 0.9 || uv.y > 0.9) {
            oColor.rgb = vec3(0.0);
        }
    }
    
    oColor.a = 1.0; // AO
    
    // Wet Soil (Shifted warmer)
    float wetnessMult = clamp(-fHeight * 10.0 + 0.01, 0.0, 1.0);
    oColor.rgb = mix(oColor.rgb, oColor.rgb * 0.6 * vec3(1.2, 1.1, 1.0), wetnessMult);
    
    
    // === Roughness + metallic ===
    oMetallicRoughness.r = 0.0; // Metallic
	oMetallicRoughness.g = 1.0 - texture(GreyNoise, fUV * 16.0).r * 0.3; // Roughness
    // TODO: Cosine curve so only shore is wet?
    oMetallicRoughness.g = max(oMetallicRoughness.g - wetnessMult * 0.3, 0.0);
    
    // Debug draw cell noise
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(cellNoiseColor, cellNoiseColor, cellNoiseColor);
    
    // Debug distance lerp
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(distUvLerp, 0.0, 0.0);

}