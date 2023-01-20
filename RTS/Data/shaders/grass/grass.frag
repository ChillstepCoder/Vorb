#include "../GlobalUbo.glsl"
#include "AlphaTest.glsl"

uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform float unFadeDistance = 1000.0;
uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

in vec3 fPosition;
in vec2 fUV;
flat in float fAtlasPage;
in float fDistance;

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec3 oRoughness;

float InvSmoothStep(float x) {
    return x + (x - (x * x * (3.0 - 2.0 * x)));
}

void main() {
    vec4 color = texture(GrassTexture, fUV).rgba;
    // TODO: Lower settings disable transparency?
	
	// Distance fade
	float noiseVal = texture(GreyNoise, (fPosition.xy + CameraPos.xy) * 0.05).r;
	float fadeDist = unFadeDistance * 0.35;
	float lerpVal = clamp(fDistance, 0.0, fadeDist) / fadeDist;
	color.a *= clamp(mix(0.0, 1.0, 1.0 - ((noiseVal  + 1.0) * lerpVal)), 0.0, 1.0);
	
	// Crossfade
	float alpha = unCrossfadeAlpha * 0.05 + noiseVal * unCrossfadeAlpha + 0.4;
	alpha = InvSmoothStep(alpha);
	color.a = min(mix(1.0 - alpha, alpha, unCrossfadeDirection), color.a);
	color.a = clamp(color.a, 0.0, 1.0);
	
    runAlphaTest(color.a, 0.01);
    oColor = color.rgb;
	oNormal.rgb = vec3(0.5, 0.5, 1.0);
	oRoughness.r = 0.75;
}