uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;
in vec3 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    oColor.rgb = texture(Atlas, vec3(fUV, fAtlasPage)).rgb;
	oColor.rgb *= fTint;
	oColor.a = 1.0;
	
	// TMP
	oColor.rgb = oColor.rgb * 0.000001 + vec3(1.0, 0.0, 0.0);
	
	
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