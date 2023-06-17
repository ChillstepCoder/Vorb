//#include "util/hsv.glsl"
#include "GlobalUbo.glsl"


uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform sampler2D GrassGradients;
uniform sampler2D TerrainGradients;
uniform sampler2D CellNoise;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 100.0 / 255.0, 155.0 / 255.0);
uniform vec3 StoneColor = vec3(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);
uniform float unColorMapScale = 0.1;

uniform float unHeightMult = 0.191;
uniform float unWavyMult = 0.167;
uniform float unSquaresIntensity = 0.5;
uniform float unSquaresPeriod = 0.187;
uniform float unBlendMult = 0.037;
uniform float unGrassColorV = 0.2;

uniform int unDebugLines = 0;
uniform int unUseNewGradient = 1;

in float fHeight;
in vec3 fPosition;
in vec2 fUV;
in mat3 fTBN;

uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

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
    
    // Grass color
    
    vec2 farStoneUVs = -(fUV * 0.01);
    vec2 farGrassUVs = -(fUV * 0.1);
    float distance = length(fPosition.xy);
    float distUvLerp = min(distance * 0.001, 1.0);
    
    float cellNoiseColor = texture(CellNoise, fUV * unColorMapScale).r;
    vec2 gradientUV = vec2(1.0 - cellNoiseColor, unGrassColorV);
    vec3 GrassColor;
    if (unUseNewGradient == 1){
        GrassColor = texture(TerrainGradients, gradientUV).rgb;
    } else {
        GrassColor = texture(GrassGradients, gradientUV).rgb;
    }
    
    // === Normals ===
    // TODO: NORMAL MAPPING
    vec3 normal = vec3(0.0, 0.0, 1.0);
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
    
    // Debug distance lerp
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(distUvLerp, 0.0, 0.0);
    
    // =========== BEGIN NEW ART STYLE ==========
    
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
    
    // TODO: REVISIT THIS!!!!!
    if (fHeight > 15.0) {
        oColor.rgb = mix(COLORS[index], COLORS[index2], lerpVal); // PRETTY RAINBOW + 0.5 * vec3((cos(fUV.x * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1) + 1.0) * 0.5, (cos(fUV.y * 0.1 - fUV.x * 0.1) + 1.0) * 0.5);
    } else {
        oColor.rgb = GrassColor;
    }
  
    
    // =========== END NEW ART STYLE ==========
    // TMP Texturing test with color remapping
    // We use the texture value with the terrain color and saturation
    vec3 textureColor = mix(texture(GrassTexture, fUV).rgb, texture(GrassTexture, farGrassUVs).rgb, distUvLerp);
    //vec3 currhsv = rgb2hsv(oColor.rgb);
    //vec3 texturehsv = rgb2hsv(textureColor);
    //currhsv.b = texturehsv.b;
    //currhsv.b = mix(currhsv.b, texturehsv.b, 0.4);
    //oColor.rgb = hsv2rgb(currhsv);
    
    if (unUseNewGradient == 1){
        float v = textureColor.r;
        vec2 uv = vec2(gradientUV.x, v);
        oColor.rgb = texture(TerrainGradients, uv).rgb;
    } else {
        oColor.rgb *= textureColor;
    }
    if (unDebugLines == 1)
    {
        vec2 scaledUV = fUV * 0.3;
        vec2 uv = vec2(fract(scaledUV.x), fract(scaledUV.y));
        if (uv.x > 0.9 || uv.y > 0.9) {
            oColor.rgb = vec3(0.0);
        } else {
            vec2 uv2 = vec2(uv.x * (1.0 / 0.9), textureColor.r);
            oColor.rgb = texture(TerrainGradients, uv2).rgb;
        }
    }
    
    oColor.a = 1.0; // AO
    
    // Wet Soil (Shifted warmer)
    float wetnessMult = clamp(-fHeight * 10.0 + 0.01, 0.0, 1.0);
    oColor.rgb = mix(oColor.rgb, oColor.rgb * 0.6 * vec3(1.2, 1.1, 1.0), wetnessMult);
    
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(cellNoiseColor, cellNoiseColor, cellNoiseColor);
    
    // === Roughness + metallic ===
    oMetallicRoughness.r = 0.0; // Metallic
	oMetallicRoughness.g = 1.0 - texture(GreyNoise, fUV * 16.0).r * 0.3; // Roughness
    // TODO: Cosine curve so only shore is wet?
    oMetallicRoughness.g = max(oMetallicRoughness.g - wetnessMult * 0.3, 0.0);
}