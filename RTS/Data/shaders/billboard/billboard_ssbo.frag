#include "MaterialData.glsl"
#include "BillboardSSBO.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in float fRoughness;

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec3 oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    vec4 color = sampleMaterialAlbedo(mtl, fUV) * fTint;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	
    if (color.a < 0.70) {
        discard;
    }
    oColor = color.rgb;
	
	
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
	oRoughness.r = fRoughness;
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(1.0, 0.0, 0.0);
}