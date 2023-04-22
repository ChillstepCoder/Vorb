#include "MaterialData.glsl"
#include "../GlobalUbo.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassGradients;
uniform sampler2D CellNoise;
uniform float unFadeDistance = 1000.0;
uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)
uniform float unDitherPower = 0.5;
uniform float unColorMapScale = 0.1;
const int NUM_GRASS_MATERIALS = 32;

uniform int unDebugLines = 0;

in vec3 fRootPosition;
in vec2 fUV;
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
    
    vec2 worldUV = fRootPosition.xy * 0.05;
    float cellNoiseColor = texture(CellNoise, worldUV * unColorMapScale).r;
    vec2 gradientUV = vec2(1.0 - cellNoiseColor, fUV.y);
    vec3 GrassColor = texture(GrassGradients, gradientUV).rgb;
    
    MaterialData mtl = inMaterials[fGrassMaterial];
    vec4 textureColor = sampleMaterialAlbedo(mtl, fUV);
    
    color.rgb = GrassColor;
    color.a = textureColor.r;
	
	// Distance fade
	float noiseVal = texture(GreyNoise, worldUV).r;
	float fadeDist = unFadeDistance * 0.35;
	float lerpVal = clamp(fDistance, 0.0, fadeDist) / fadeDist;
	color.a *= clamp(mix(0.0, 1.0, 1.0 - ((noiseVal  + 1.0) * lerpVal)), 0.0, 1.0);
	
	// Crossfade
	float alpha = unCrossfadeAlpha * 0.05 + noiseVal * unCrossfadeAlpha + 0.4;
	alpha = InvSmoothStep(alpha);
	color.a = min(mix(1.0 - alpha, alpha, unCrossfadeDirection), color.a);
	color.a = clamp(color.a, 0.0, 1.0);
	
    runAlphaTest(pow(color.a, unDitherPower), 0.001);
    oColor.rgb = color.rgb;
    oColor.a = 1.0; // AO
    
    if (unDebugLines == 1)
    {
        vec2 scaledUV = worldUV * 0.3;
        vec2 uv = vec2(fract(scaledUV.x), fract(scaledUV.y));
        if (uv.x > 0.9 || uv.y > 0.9) {
            oColor.rgb = vec3(0.0);
        } else {
            vec2 uv2 = vec2(uv.x * (1.0 / 0.9), fUV.y);
            oColor.rgb = texture(GrassGradients, uv2).rgb;
        }
    }
    
    // Normal (Upwards)
	oNormal.rgb = vec3(0.5, 0.5, 1.0);
    
    // Metallic Roughness
	oMetallicRoughness.r = 0.0;
	oMetallicRoughness.g = 0.85;
}