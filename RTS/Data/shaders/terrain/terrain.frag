//#include "util/hsv.glsl"
#include "MaterialData.glsl"
#include "GlobalUbo.glsl"

#include "terrain/biome_util.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform sampler2D CliffTexture;
uniform sampler2D CliffNormal;
uniform vec3 WaterColor = vec3(0.0 / 255.0, 100.0 / 255.0, 155.0 / 255.0);
uniform vec3 StoneColor = vec3(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);

uniform sampler2D unSplatTexture;
// TODO: Splat data

uniform uint unSplatMaterials[256];

uniform float unHeightMult = 0.191;
uniform float unWavyMult = 0.167;
uniform float unSquaresIntensity = 0.5;
uniform float unSquaresPeriod = 0.187;
uniform float unBlendMult = 0.037;
uniform float unColorMapScale = 0.005;
uniform float unCliffBlendHardness = 80.0;
uniform float unCliffAmount = 0.2;
uniform float unCliffZMult = 1.2;

// Config
uniform int unDebugLines = 0;

const float DISTANT_COLOR_EXP = 0.6;
const float DISTANT_COLOR_INTENSITY = 0.75;

in float fHeight;
in vec3 fPosition;
in vec2 fBiomeUV;
in vec2 fUV;
in mat3 fTBN;
in float fSnow;
in vec3 fNormal;
in vec3 fFragPosTangent;
in vec2 fSplatUV;

uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

// Debug colors
const vec3 BIOME_COLORS[4] = {
    vec3(1.0, 0.0, 0.0), // PLAINS
    vec3(1.0, 0.0, 1.0), // MOUNTAINS
    vec3(0.0, 1.0, 0.0), // FOREST
    vec3(0.0, 1.0, 1.0), // HOT SPRINGS
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

void computeCrossfade() {
    
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
}

vec3 computeNormal() {
    // TODO: NORMAL MAPPING
    return normalize(fTBN[2]);
}

float getTerrainDistanceFactor(float distance) {
    return min(distance * 0.001, 1.0);
}

vec3 getTerrainColor(vec2 terrainUvs, int biome) {
    float detailValue = texture(GrassTexture, terrainUvs * 10.0).r * unDetailTextureStrength;
    float u = getBiomeColorGradientUCoord(terrainUvs);
    float v = detailValue;
    vec2 uv = vec2(u, v);
    return texture(unBiomeColorMapsTexture, vec3(uv, float(biomeColorMapLookup[biome]))).rgb;
}

float getLuminance(vec3 color) {
    return (0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b);
}

// https://bgolus.medium.com/normal-mapping-for-a-triplanar-shader-10bf39dca05a#38e5
vec3 getTriPlanarBlend(vec3 norm, float lumaX, float lumaY, float noiseBlend) {
	// Asymmetric Triplanar Blend
    vec3 blend = vec3(0.0); // Blend for sides only
    vec2 xyBlend = normalize(abs(norm.xy));
    blend.xy = max(vec2(0.0), xyBlend - vec2(0.67));
    blend.xy /= max(0.00001, dot(blend.xy, vec2(1,1)));// Blend for top
    
    
    // Luminance of cliff affects blend
    noiseBlend += max(lumaX * blend.x, lumaY * blend.y) * 15.0;
    blend.z = clamp((abs(norm.z) - unCliffAmount) * unCliffBlendHardness + noiseBlend, 0.0, 1.0);
    blend.xy *= (1.0 - blend.z);
    return blend;
}
vec3 getTriplanarNormal(vec3 surfaceNorm, vec2 uvX, vec2 uvY, vec3 blend) {
    // Whiteout blend

    // Tangent space normal maps
    vec3 tnormalX = texture(CliffNormal, uvX).rgb * 2.0 - vec3(1.0);
    vec3 tnormalY = texture(CliffNormal, uvY).rgb * 2.0 - vec3(1.0);
    vec3 tnormalZ = vec3(0.0, 0.0, 1.0);

    // Swizzle world normals into tangent space and apply Whiteout blend
    tnormalX = vec3(
        tnormalX.xy + surfaceNorm.zy,
        abs(tnormalX.z) * surfaceNorm.x
    );
    tnormalY = vec3(
        tnormalY.xy + surfaceNorm.xz,
        abs(tnormalY.z) * surfaceNorm.y
    );
    tnormalZ = vec3(
        tnormalZ.xy + surfaceNorm.xy,
        abs(tnormalZ.z) * surfaceNorm.z
    );

    // Swizzle tangent normals to match world orientation and triblend
    vec3 worldNormal = normalize(
        tnormalX.zyx * blend.x +
        tnormalY.xzy * blend.y +
        tnormalZ.xyz * blend.z
    );
    
    return worldNormal.xyz;
}

vec3 heightblend(vec3 input1, float height1, vec3 input2, float height2) {
    float BLEND_FACTOR = 0.4;
    float height_start = max(height1, height2) - BLEND_FACTOR;
    float level1 = max(height1 - height_start, 0);
    float level2 = max(height2 - height_start, 0);
    return ((input1 * level1) + (input2 * level2)) / (level1 + level2);
}

// =========== MAIN ===========
void main() {
	
    // === Crossfade ===
    computeCrossfade();
    
    // === Normals ===
    vec3 surfaceNormal = computeNormal();
    
    // === Biome ===
    int biome = getBiome(fBiomeUV);

    // === Terrain Color ===
    float distance = length(fPosition.xy);
    float distanceFactor = getTerrainDistanceFactor(distance);
    oColor.rgb = getTerrainColor(fUV, biome);
    
    // =========== Cliff color ===========
    const float CLOSE_MULT = 10.0;
    const float FAR_MULT = 3.0;
    vec2 xyUVClose = fUV.xy * CLOSE_MULT;
    vec2 xyUVFar = fUV.xy * FAR_MULT;
    float heightV = fHeight * unColorMapScale * unCliffZMult;
    float heightVClose = heightV * CLOSE_MULT;
    float heightVFar = heightV * FAR_MULT;
    vec3 xSampleClose = texture(CliffTexture, vec2(xyUVClose.y, heightVClose)).rgb;
    vec3 ySampleClose = texture(CliffTexture, vec2(xyUVClose.x, heightVClose)).rgb;
    
    // Far
    vec3 xSampleFar = texture(CliffTexture, vec2(xyUVFar.y, heightVFar)).rgb;
    vec3 ySampleFar = texture(CliffTexture, vec2(xyUVFar.x, heightVFar)).rgb;
    
    
    // Turbulent noise is from biome_util.glsl
    float rawTurb = texture(TurbulentNoise, fUV * 4.0).r;
    float noiseBlend = (rawTurb * 2.0 - 1.0) * 4.0;
    vec3 weights = getTriPlanarBlend(surfaceNormal.rgb, getLuminance(xSampleClose), getLuminance(xSampleClose), noiseBlend);
    vec3 cliffClose = weights.x * xSampleClose + weights.y * ySampleClose;
    vec3 cliffFar = weights.x * xSampleFar + weights.y * ySampleFar;
    float cliffDistFactor = min(distance * 0.01, 1.0);
    oColor.rgb = oColor.rgb * weights.z + mix(cliffClose, cliffFar, cliffDistFactor);
    
    // =========== Output Normals ===========
    vec3 normalClose = getTriplanarNormal(surfaceNormal, vec2(xyUVClose.y, heightVClose), vec2(xyUVClose.x, heightVClose), weights);
    vec3 normalFar = getTriplanarNormal(surfaceNormal, vec2(xyUVFar.y, heightVFar), vec2(xyUVFar.x, heightVFar), weights);
    vec3 finalNormal = mix(normalClose, normalFar, cliffDistFactor);
    finalNormal = mix(finalNormal, surfaceNormal, min(fSnow, 1.0));

	//oNormal.rgb = oNormal.rgb * 0.00001 + (surfaceNormal + 1.0) * 0.5;
    
    // =========== Distance stylized color ===========
    float FLAT_REDUCE_MULT = 1.0;
    float GRANULARITY_MULT = unWavyMult;
    float GRANULARITY = 12.0 * GRANULARITY_MULT * (1.0 - surfaceNormal.z * FLAT_REDUCE_MULT);
    float UV_MOD_MULT = unSquaresPeriod;
    float uvMod = 0.5 + (triangularWave(fUV.x * UV_MOD_MULT) + triangularWave(fUV.y * UV_MOD_MULT)) * unSquaresIntensity * 2.0;
    float HEIGHT_ADD = (fHeight * uvMod) * 0.024 * unHeightMult * 2.0;
    surfaceNormal = vec3(abs(surfaceNormal.x) * GRANULARITY + HEIGHT_ADD, abs(surfaceNormal.y) * GRANULARITY + HEIGHT_ADD, surfaceNormal.z * GRANULARITY);
    
    float totalNormal = surfaceNormal.x + surfaceNormal.y;
    
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
    oColor.rgb = mix(oColor.rgb, distanceColor, pow(distanceFactor, DISTANT_COLOR_EXP) * DISTANT_COLOR_INTENSITY);
        
    // =========== Wet Soil ===========
    // Shift warmer
    float wetnessMult = clamp(-fHeight * 10.0 + 0.01, 0.0, 1.0);
    oColor.rgb = mix(oColor.rgb, oColor.rgb * 0.6 * vec3(1.2, 1.1, 1.0), wetnessMult);
    
    // === Roughness + metallic ===
    oMetallicRoughness.r = 0.0; // Metallic
	oMetallicRoughness.g = 1.0 - texture(GreyNoise, fUV * 16.0).r * 0.3; // Roughness
    // TODO: Cosine curve so only shore is wet?
    oMetallicRoughness.g = max(oMetallicRoughness.g - wetnessMult * 0.3, 0.0);
    // Increase total roughness (talia request)
    oMetallicRoughness.g = min(oMetallicRoughness.g + 0.5, 1.0);
    
    oColor.a = 1.0; // AO?
    
    
    vec2 splatOffset = vec2(rawTurb);
    float splatValue = texture(unSplatTexture, fSplatUV + splatOffset * 0.01).r;
    
    if (splatValue != 0.0) {
        uint splatMaterialId = unSplatMaterials[uint(splatValue * 255.0)];
        
         vec3 normal;
        vec4 roadSample;
        float ao;
        float metallic;
        float roughness;
        vec2 uv = fUV * 100.0;
        // 0.4 matches height blend scale
        
        float intensity = 1.0;
        
        // Disp
        MaterialData mtl = inMaterials[splatMaterialId];
        if (mtl.displacementMap > 0) {
            float heightScale = max((intensity - 0.45) * 0.6, 0);
            vec3 VIEW_POS_TANGENT = vec3(0.0,0.0,0.0); // Camera is at 0!
            vec3 tangentViewDir = normalize(VIEW_POS_TANGENT - fFragPosTangent);
            uv = dispMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir, heightScale);
        }
        
        getMaterialPixelInfo(splatMaterialId, uv, roadSample, normal, ao, metallic, roughness, vec4(1.0,1.0,1.0,1.0));
        
        //vec3 roadSample = texture(DirtRoad, fUV * 10.0).rgb;
        float roadLuminance = getLuminance(roadSample.rgb) * 4.0;
        // Fake shitty height learp (Adjust based on texture?)
        //float roadBlend = clamp(intensity * 2 - pow(roadLuminance, 4.0), 0.0, 1.0);
        float roadBlend = clamp(pow(intensity, 0.001 + rawTurb * 3.0), 0.0, 1.0);
        //roadBlend = intensity;
        oColor.rgb = heightblend(oColor.rgb, 1.0 - roadBlend, roadSample.rgb, roadLuminance * roadBlend);
        //oColor.rgb = mix(oColor.rgb, roadSample, roadBlend);
       // oColor.rgb = 0.0001 * oColor.rgb + fFragPosTangent.z * 0.1;
       
       // Normal map
       vec3 roadNormal = fTBN * normal;
       finalNormal = mix(finalNormal, roadNormal, roadBlend);
       
       // Roughness metallic
       oMetallicRoughness.rg = mix(oMetallicRoughness.rg, vec2(metallic, roughness), roadBlend); 
       
       // Ambient occlusion
       oColor.a = mix(oColor.a, ao, roadBlend); 
       
       
      // oColor.rgb = 0.0001 * oColor.rgb + vec3(h);
       
       // Uncomment to debug weird negative frag pos issue
       // if (-fFragPosTangent.z < 0.0) {
       //     oColor.r = 1.0;
       // }
    }
    
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(fSplatUV.x, 0.0, 0.0);
    
    
    // =========== Snow ===========
    oColor.rgb = mix(oColor.rgb, vec3(1.0), min(fSnow * 6.0, 1.0));
    
    // Debug biome colors 
    // if (biome < 4) oColor.rgb = mix(oColor.rgb, BIOME_COLORS[biome], 1.0);
    
    // Debug draw cell noise
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(cellNoiseColor, cellNoiseColor, cellNoiseColor);
    
    // Debug distance lerp
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(distanceFactor, 0.0, 0.0);
       
    if (unDebugLines == 1)
    {
        vec2 scaledUV = fUV * 0.3;
        vec2 uv = vec2(fract(scaledUV.x), fract(scaledUV.y));
        if (uv.x > 0.9 || uv.y > 0.9) {
            oColor.rgb = vec3(0.0);
        }
    }
    //oColor.rgb = 0.0001 * oColor.rgb + oNormal.rgb;//fNormal.rgb * 0.5 + 0.5;
    
    oNormal.rgb = (finalNormal + 1.0) * 0.5;
}
