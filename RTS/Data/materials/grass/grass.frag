uniform sampler2DArray Atlas;
uniform sampler2D GreyNoise;
uniform sampler2D GrassTexture;
uniform float unFadeDistance = 1000.0;
uniform float unCrossfadeAlpha = 0.0;
uniform float unCrossfadeDirection = 1.0; // Either 0.0 (out) or 1.0 (in)

in vec2 fScreenUV;
in vec2 fUV;
flat in float fAtlasPage;
in mat3 fTBN;
in float fRoughness;
in float fDistance;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

float InvSmoothStep(float x) {
    return x + (x - (x * x * (3.0 - 2.0 * x)));
}

void main() {
    oColor.rgba = texture(GrassTexture, fUV).rgba;
    // TODO: Lower settings disable transparency?
	
	// Distance fade
	float noiseVal = texture(GreyNoise, fScreenUV * 3.0).r;
	float fadeDist = unFadeDistance * 0.35;
	float lerpVal = clamp(fDistance, 0.0, fadeDist) / fadeDist;
	oColor.a *= clamp(mix(0.0, 1.0, 1.0 - ((noiseVal  + 1.0) * lerpVal)), 0.0, 1.0);
	
	// Crossfade
	float alpha = unCrossfadeAlpha * 0.05 + noiseVal * unCrossfadeAlpha + 0.4;
	alpha = InvSmoothStep(alpha);
	oColor.a = min(mix(1.0 - alpha, alpha, unCrossfadeDirection), oColor.a);
	oColor.a = clamp(oColor.a, 0.0, 1.0);
	
	if (oColor.a <= 0.5) {
        discard;
    }
	
	// Normal is always the next page
	//vec3 normal = texture(Atlas, vec3(fScreenUV, fAtlasPage + 1.0)).rgb;
	//normal = normal * 2.0 - 1.0;
	//normal = normalize(fTBN * normal);
	vec3 normal = normalize(fTBN * vec3(0.0, 0.0, 1.0));
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
	oRoughness.r = fRoughness;
	oRoughness.a = 1.0;
}