uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    oColor = texture(Atlas, vec3(fUV, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	
    if (oColor.a < 0.85) {
        discard;
    }
	oColor.a = 1.0;
	
	
	// Normal is always the next page
	vec3 normal = texture(Atlas, vec3(fUV, fAtlasPage + 1.0)).rgb;
	normal = normal * 2.0 - 1.0;
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
	oRoughness.r = fRoughness;
	oRoughness.a = 1.0;
}