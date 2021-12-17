uniform sampler2DArray Atlas;
uniform sampler2D GreyNoise;
uniform float unFadeDistance = 1000.0;

in vec2 fUV;
flat in float fAtlasPage;
in vec3 fTint;
in mat3 fTBN;
in float fRoughness;
in float fDistance;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    oColor.rgba = texture(Atlas, vec3(fUV, fAtlasPage)).rgba;
	// TODO: Lower settings disable transparency?
	
	oColor.rgb *= fTint;
	oColor.a = 1.0;
	
	// TMP (need texture)
	oColor.rgb = oColor.rgb * 0.000001 + vec3(167.0 / 255.0, 163.0 / 255.0, 112.0 / 255.0);
	
	float noiseVal = texture(GreyNoise, fUV * 3.0).r + 1.0;
	float lerpVal = clamp(fDistance, 0.0, unFadeDistance) / unFadeDistance;
	oColor.a = clamp(mix(0.0, 1.0, 1.0 - (noiseVal * lerpVal)), 0.0, 1.0);

	
	if (oColor.a < 0.85) {
        discard;
    }
	
	// Normal is always the next page
	//vec3 normal = texture(Atlas, vec3(fUV, fAtlasPage + 1.0)).rgb;
	//normal = normal * 2.0 - 1.0;
	//normal = normalize(fTBN * normal);
	vec3 normal = normalize(fTBN * vec3(0.0, 0.0, 1.0));
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
	oRoughness.r = fRoughness;
	oRoughness.a = 1.0;
}