#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "GrassUbo.glsl"
#include "terrain/biome_util.glsl"

uniform sampler2D GreyNoise;
uniform float unFadeDistance = 1000.0;
uniform float unCrossfadeAlpha = 0.0;
uniform float unColorMapScale = 0.005;

uniform int unDebugLines = 0;

uniform vec2 unUVRoot;
uniform float unSnowLevel;

flat in vec2 fRelXY;
in vec2 fUV;
flat in int fBiome;
flat in int fGrassMaterial;
in float fDistance;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

float InvSmoothStep(float x) {
    return x + (x - (x * x * (3.0 - 2.0 * x)));
}

void main() {
    vec4 color;
    // TODO: Lower settings disable transparency?
    
    // TODO: Fix
    vec2 worldUV = unUVRoot + fRelXY.xy * unColorMapScale;
    float colorU = getBiomeColorGradientUCoord(worldUV);
    vec2 gradientUV = vec2(colorU, fUV.y);
    
    int materialID = unGrassData[fGrassMaterial].material;
    MaterialData mtl = inMaterials[materialID];
    vec4 textureColor = sampleMaterialAlbedo(mtl, fUV);
    
    if (unGrassData[fGrassMaterial].shouldUseColorGradient == 1) {
        color.rgb = texture(unBiomeColorMapsTexture, vec3(gradientUV, float(biomeColorMapLookup[fBiome]))).rgb;
        color.a = textureColor.r;
    } else {
        color = textureColor;
    }
    
	
	// Distance fade
    // TODO: Can we do without a noise lookup?
	float noiseVal = texture(GreyNoise, worldUV * 20.0).r;
	float fadeDist = unFadeDistance * 0.35;
	float lerpVal = clamp(fDistance, 0.0, fadeDist) / fadeDist;
	color.a *= clamp(mix(0.0, 1.0, 1.0 - ((noiseVal + 1.0) * lerpVal)), 0.0, 1.0);
	
	// Crossfade
    runAlphaTestWithCrossfade(color.a, unCrossfadeAlpha);
    
    oColor.rgb = color.rgb;
    oColor.a = 1.0; // AO
    
    if (unDebugLines == 1)
    {
        vec2 scaledUV = worldUV * 0.3;
        vec2 uv = vec2(fract(scaledUV.x), fract(scaledUV.y));
        if (uv.x > 0.9 || uv.y > 0.9) {
            oColor.rgb = vec3(0.0);
        }
    }
    
    //oColor.rgb = 0.0001 * oColor.rgb + vec3(unCrossfadeAlpha, alphaThreshold, 0.0);

    // =========== Snow ===========
    oColor.rgb = mix(oColor.rgb, vec3(1.0), unSnowLevel);
    
    // Normal (Upwards)
	oNormal.rgb = vec3(0.5, 0.5, 1.0);
    
    // Metallic Roughness
	oMetallicRoughness.r = 0.0;
	oMetallicRoughness.g = 0.85;
}