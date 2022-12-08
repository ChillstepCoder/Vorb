#include "MaterialData.glsl"
#include "BillboardSSBO.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    oColor = sampleMaterialAlbedo(mtl, fUV) * fTint;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	
    if (oColor.a < 0.70) {
        discard;
    }
	oColor.a = 1.0;
	
	
	// Normal is always the next page
    vec3 normal;
    if (mtl.normalMap > 0) {
        vec3 normal = sampleMaterialNormal(mtl, fUV).rgb;
        normal = normal * 2.0 - 1.0;
    } else {
        normal = vec3(0.0, 0.0, 1.0);
    }
    
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	oNormal.a = oColor.a;
	oRoughness.r = fRoughness;
	oRoughness.a = 1.0;
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(1.0, 0.0, 0.0);
}