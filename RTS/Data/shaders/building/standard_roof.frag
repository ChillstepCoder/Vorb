uniform sampler2DArray Atlas;

in vec2 fUV;
in vec4 fUVTiling;
in vec3 fNormal;
flat in float fAtlasPage;
in vec4 fTint;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;

void main() {
    vec2 frac = fract(fUV);

    oColor = texture(Atlas, vec3(fUVTiling.xy + fUVTiling.zw * frac, fAtlasPage)) * fTint;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	//oColor.rgb = oColor.rgb * 0.0001 + (fNormal + vec3(1.0)) * 0.5;
	
	// Normal is always the next page
	// TODO: Normal mapping
	oNormal.rgb = (fNormal + 1.0) * 0.5;//;texture(Atlas, vec3(fUV, fAtlasPage + 1.0));
	oNormal.a = oColor.a;
}